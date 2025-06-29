/**
 * Vibe Architect Extension
 *
 * Entry point for the Vibe Architect VS Code extension.
 * Responsible for establishing a WebSocket connection to the Vibe Server and handling incoming commands.
 *
 * @module extension
 */

import * as vscode from 'vscode';
import WebSocket from 'ws';

/**
 * Activates the extension when VS Code loads it.
 * Sets up a WebSocket connection to the Vibe Server and defines event handlers for WebSocket lifecycle events.
 *
 * @param context - The VS Code extension context
 */
export function activate(context: vscode.ExtensionContext): void {
    console.log('Vibe Architect: Activado y listo para recibir órdenes.');

    const serverUri = 'ws://localhost:8000/ws';
    const ws = new WebSocket(serverUri);

    // WebSocket event handlers
    ws.on('open', () => {
        vscode.window.showInformationMessage('Vibe: Conectado al Cerebro.');
        console.log(`Vibe Architect: Conexión establecida con ${serverUri}`);
    });

    ws.on('message', (message: WebSocket.Data) => {
        handleServerCommand(message);
    });

    ws.on('close', () => {
        vscode.window.showWarningMessage('Vibe: Desconectado del Cerebro.');
        console.log('Vibe Architect: Conexión cerrada.');
    });

    ws.on('error', (error) => {
        vscode.window.showErrorMessage('Vibe: Error de conexión con el Cerebro. ¿Está Vibe Server activo?');
        console.error('Vibe Architect: Error en WebSocket -', error.message);
    });
}

/**
 * Processes commands received from the Vibe Server.
 * Adheres to the Single Responsibility Principle (SRP) by isolating command-handling logic.
 *
 * @param message - The raw message received from the WebSocket
 */
function handleServerCommand(message: WebSocket.Data): void {
    try {
        const command = JSON.parse(message.toString());

        // Route commands based on the 'action' property
        switch (command.action) {
            case 'create_file':
                createFile(command.params);
                break;
            default:
                console.warn(`Vibe Architect: Acción desconocida recibida: ${command.action}`);
        }
    } catch (error) {
        console.error('Vibe Architect: No se pudo parsear el comando JSON.', error);
    }
}

/**
 * Creates a file in the current workspace.
 *
 * @param params - Object containing `filename` and `content` properties
 * @param params.filename - The name of the file to create
 * @param params.content - The content to write to the file
 */
async function createFile(params: { filename: string; content: string }): Promise<void> {
    if (!params.filename) {
        vscode.window.showErrorMessage('Vibe: La orden para crear archivo no especificó un nombre.');
        return;
    }

    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) {
        vscode.window.showErrorMessage('Vibe: Debes abrir una carpeta para poder crear un archivo.');
        return;
    }

    const rootPath = workspaceFolders[0].uri;
    const filePath = vscode.Uri.joinPath(rootPath, params.filename);
    const contentBytes = new TextEncoder().encode(params.content || '');

    try {
        await vscode.workspace.fs.writeFile(filePath, contentBytes);
        vscode.window.showInformationMessage(`Vibe: Archivo "${params.filename}" creado.`);
    } catch (error) {
        vscode.window.showErrorMessage(`Vibe: Fallo al crear el archivo "${params.filename}".`);
        console.error('Vibe Architect: Error en writeFile -', error);
    }
}

/**
 * Deactivates the extension when VS Code unloads it.
 * Currently a no-op, as no cleanup is required.
 */
export function deactivate(): void { }