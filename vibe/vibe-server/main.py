"""
Vibe Server: A WebSocket-based chat server using FastAPI.

This module implements a simple WebSocket server that allows multiple clients to connect
and broadcast text messages to all connected clients in real-time.

Dependencies:
- fastapi: Provides the FastAPI framework for building the WebSocket server
- uvicorn: ASGI server implementation for running the FastAPI application

Global Variables:
- app: FastAPI instance configured with title "Vibe Server"
- active_connections: Set maintaining active WebSocket connections
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn

app = FastAPI(title="Vibe Server")
active_connections = set()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for handling client connections and messages.

    Args:
        websocket (WebSocket): The WebSocket connection instance for the client

    Behavior:
        1. Accepts incoming WebSocket connection
        2. Adds connection to active_connections set
        3. Listens for text messages from the client
        4. Broadcasts received messages to all connected clients
        5. Handles client disconnection and cleanup

    Exceptions:
        WebSocketDisconnect: Caught when a client disconnects to perform cleanup
    """
    await websocket.accept()
    print("INFO:    Conexión WebSocket aceptada.")
    active_connections.add(websocket)

    try:
        while True:
            data = await websocket.receive_text()
            print(f"INFO:   Mensaje recibido: {data}")
            for connection in active_connections:
                await connection.send_text(data)
    except WebSocketDisconnect:
        print("INFO:    Cliente desconectado.")
        active_connections.remove(websocket)

if __name__ == "__main__":
    """
    Main entry point for running the WebSocket server.

    Runs the FastAPI application using Uvicorn server on localhost (127.0.0.1)
    at port 8000.
    """
    uvicorn.run(app, host="127.0.0.1", port=8000)