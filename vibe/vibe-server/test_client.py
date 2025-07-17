"""
Vibe Server Test Client

This script serves as a debugging tool to simulate a client (e.g., Vibe Architect) connecting to the Vibe Server.
It sends a natural language command and awaits a structured JSON response to verify server functionality.

Dependencies:
- asyncio: For asynchronous I/O operations
- websockets: For establishing WebSocket connections

Functions:
- test_natural_language_command: Connects to the server, sends a natural language command, and receives a response
"""

import asyncio
import websockets

async def test_natural_language_command():
    """
    Connects to the Vibe Server, sends a natural language command, and awaits a structured JSON response.

    Behavior:
        1. Establishes a WebSocket connection to ws://localhost:8000/ws
        2. Sends a natural language command as text
        3. Prints the sent command and received JSON response
        4. Handles connection errors gracefully

    Exceptions:
        ConnectionRefusedError: Caught when the server is unreachable

    Example Command:
        "crea un nuevo archivo de configuración llamado dockerfile"
    """
    uri = "ws://localhost:8000/ws"
    try:
        async with websockets.connect(uri) as websocket:
            print("INFO:    Inspector conectado al servidor.")

            # Define a natural language command
            natural_command = "crea un nuevo archivo de configuración llamado dockerfile"

            await websocket.send(natural_command)
            print(f"INFO:    Enviando comando de lenguaje natural -> '{natural_command}'")

            # Await server response
            response_json = await websocket.recv()
            print(f"INFO:    Respuesta JSON recibida <- {response_json}")

    except ConnectionRefusedError:
        print("ERROR:   La conexión fue rechazada. ¿Está el Vibe Server en funcionamiento?")

if __name__ == "__main__":
    """
    Main entry point for running the test client.

    Executes the test_natural_language_command coroutine using asyncio.
    """
    asyncio.run(test_natural_language_command())