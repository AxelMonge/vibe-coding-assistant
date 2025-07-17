from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn
import json
import asyncio
import logging
from typing import Set
from services import gemini_service, stt_service

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = FastAPI(title="Vibe Server")
active_connections: Set[WebSocket] = set()

async def broadcast_message(message: dict, connections: Set[WebSocket]) -> None:
    message_str = json.dumps(message)
    for connection in connections.copy():
        try:
            await asyncio.wait_for(connection.send_text(message_str), timeout=5.0)
        except (asyncio.TimeoutError, WebSocketDisconnect):
            logger.warning("Failed to send message to a connection")
            connections.discard(connection)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    try:
        await asyncio.wait_for(websocket.accept(), timeout=5.0)
        logger.info("WebSocket connection accepted")
        active_connections.add(websocket)

        while True:
            try:
                # Recibir datos binarios (audio)
                audio_data = await asyncio.wait_for(websocket.receive_bytes(), timeout=30.0)
                logger.info("Received audio data")

                # Transcribir audio a texto
                transcribed_text = stt_service.transcribe_audio_bytes(audio_data)
                if not transcribed_text:
                    logger.error("Failed to transcribe audio")
                    error_message = {"action": "error", "params": {"message": "Audio transcription failed"}}
                    await broadcast_message(error_message, active_connections)
                    continue

                logger.info(f"Transcribed text: '{transcribed_text}'")

                # Procesar texto transcrito con Gemini
                try:
                    structured_command = await asyncio.wait_for(
                        gemini_service.translate_command_to_json(transcribed_text),
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
    uvicorn.run(app, host="127.0.0.1", port=8000, workers=1)