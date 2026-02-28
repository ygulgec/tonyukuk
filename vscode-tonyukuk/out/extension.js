"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode_1 = require("vscode");
const node_1 = require("vscode-languageclient/node");
let client;
function activate(context) {
    const config = vscode_1.workspace.getConfiguration("tonyukuk");
    const serverPath = config.get("lspPath", "tonyukuk-lsp");
    const serverOptions = {
        command: serverPath,
        args: [],
    };
    const clientOptions = {
        documentSelector: [{ scheme: "file", language: "tonyukuk" }],
        synchronize: {
            fileEvents: vscode_1.workspace.createFileSystemWatcher("**/*.tr"),
        },
    };
    client = new node_1.LanguageClient("tonyukukLsp", "Tonyukuk Dil Sunucusu", serverOptions, clientOptions);
    client.start();
}
function deactivate() {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
//# sourceMappingURL=extension.js.map