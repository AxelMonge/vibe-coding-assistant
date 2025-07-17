"""
Vibe Server: A WebSocket-based command processing server using FastAPI.

This module implements a WebSocket server that receives user commands, processes them
using the Gemini service, and broadcasts structured JSON responses to all connected clients.

Dependencies:
- fastapi: Provides the FastAPI framework for building the WebSocket server
- uvicorn: ASGI server implementation for running the FastAPI application
- json: Handles JSON serialization for structured command responses
- services.gemini_service: Custom module for translating user commands to structured JSON
- asyncio: For asynchronous operations and timeout handling
- logging: For structured logging
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn
import json
import asyncio
import logging
from typing import Set
from services import gemini_service

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = FastAPI(title="Vibe Server")
active_connections: Set[WebSocket] = set()

async def broadcast_message(message: dict, connections: Set[WebSocket]) -> None:
    """
    Broadcasts a message to all active WebSocket connections.

    Args:
        message (dict): The message to broadcast
        connections (Set[WebSocket]): Set of active WebSocket connections
    """
    message_str = json.dumps(message)
    for connection in connections.copy():  # Copy to avoid runtime modification
        try:
            await asyncio.wait_for(connection.send_text(message_str), timeout=5.0)
        except (asyncio.TimeoutError, WebSocketDisconnect):
            logger.warning("Failed to send message to a connection")
            connections.discard(connection)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for handling client connections and command processing.

    Args:
        websocket (WebSocket): The WebSocket connection instance for the client

    Behavior:
        1. Accepts incoming WebSocket connection
        2. Adds connection to active_connections set
        3. Listens for text commands from the client with timeout
        4. Sends commands to Gemini service for translation to structured JSON
        5. Broadcasts structured JSON or error messages to all connected clients
        6. Handles client disconnection and cleanup

    Exceptions:
        WebSocketDisconnect: Caught when a client disconnects
        asyncio.TimeoutError: Caught when client communication times out
        Exception: General exception catch for unexpected errors
    """
    try:
        await asyncio.wait_for(websocket.accept(), timeout=5.0)
        logger.info("WebSocket connection accepted")
        active_connections.add(websocket)

        while True:
            try:
                user_command_text = await asyncio.wait_for(websocket.receive_text(), timeout=30.0)
                logger.info(f"Received user command: '{user_command_text}'")

                try:
                    structured_command = await asyncio.wait_for(
                        gemini_service.translate_command_to_json(user_command_text),
                        timeout=10.0
                    )
                    if structured_command:
                        logger.info(f"Translated command to JSON: {structured_command}")
                        await broadcast_message(structured_command, active_connections)
                    else:
                        logger.error("Failed to translate command")
                        error_message = {"action": "error", "params": {"message": "Command translation failed"}}
                        await broadcast_message(error_message, active_connections)
                except asyncio.TimeoutError:
                    logger.error("Gemini service timeout")
                    error_message = {"action": "error", "params": {"message": "Service timeout"}}
                    await broadcast_message(error_message, active_connections)
                except Exception as e:
                    logger.error(f"Error processing command: {str(e)}")
                    error_message = {"action": "error", "params": {"message": "Internal server error"}}
                    await broadcast_message(error_message, active_connections)

            except asyncio.TimeoutError:
                logger.warning("Client receive timeout")
                error_message = {"action": "error", "params": {"message": "Client timeout"}}
                await broadcast_message(error_message, active_connections)
                break

    except WebSocketDisconnect:
        logger.info("Client disconnected")
    except Exception as e:
        logger.error(f"WebSocket error: {str(e)}")
    finally:
        active_connections.discard(websocket)
        logger.info("Connection removed from active connections")

if __name__ == "__main__":
    """
    Main entry point for running the WebSocket server.

    Runs the FastAPI application using Uvicorn server on localhost (127.0.0.1)
    at port 8000 with specified number of workers.
    """
    uvicorn.run(app, host="127.0.0.1", port=8000, workers=1)