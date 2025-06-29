"""
Vibe Server Test Client

This script serves as a debugging tool to simulate a client (e.g., Vibe Architect) connecting to the Vibe Server.
It sends a test JSON command and awaits a response to verify server functionality.

Dependencies:
- asyncio: For asynchronous I/O operations
- websockets: For establishing WebSocket connections
- json: For serializing/deserializing JSON data

Functions:
- test_command: Connects to the server, sends a test command, and receives a response
"""

import asyncio
import websockets
import json

async def test_command():
    """
    Connects to the Vibe Server, sends a JSON test command, and awaits a response.

    Behavior:
        1. Establishes a WebSocket connection to ws://localhost:8000/ws
        2. Sends a JSON-formatted test command to create a file
        3. Prints the sent command and received response
        4. Handles connection errors gracefully

    Exceptions:
        ConnectionRefusedError: Caught when the server is unreachable

    Example Command:
        {
            "action": "create_file",
            "params": {
                "filename": "hello_vibe.txt",
                "content": "Vibe Server y Vibe Architect están conectados."
            }
        }
    """
    uri = "ws://localhost:8000/ws"
    try:
        async with websockets.connect(uri) as websocket:
            print("INFO:    Inspector conectado al servidor.")

            # Define a test command structured as a Python dictionary
            test_command = {
                "action": "create_file",
                "params": {
                    "filename": "hello_vibe.txt",
                    "content": "Vibe Server y Vibe Architect están conectados."
                }
            }

            # Serialize the dictionary to a JSON string
            command_json = json.dumps(test_command)

            await websocket.send(command_json)
            print(f"INFO:   Comando enviado -> {command_json}")

            # Await server response
            response = await websocket.recv()
            print(f"INFO:   Respuesta recibida <- {response}")
    except ConnectionRefusedError:
        print("ERROR:   La conexión fue rechazada. ¿Está el Vibe Server en funcionamiento?")

if __name__ == "__main__":
    """
    Main entry point for running the test client.

    Executes the test_command coroutine using asyncio.
    """
    asyncio.run(test_command())