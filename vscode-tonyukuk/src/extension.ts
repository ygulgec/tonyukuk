import * as path from "path";
import { workspace, ExtensionContext } from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

export function activate(context: ExtensionContext): void {
  const config = workspace.getConfiguration("tonyukuk");
  const serverPath: string = config.get<string>("lspPath", "tonyukuk-lsp");

  const serverOptions: ServerOptions = {
    command: serverPath,
    args: [],
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "tonyukuk" }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher("**/*.tr"),
    },
  };

  client = new LanguageClient(
    "tonyukukLsp",
    "Tonyukuk Dil Sunucusu",
    serverOptions,
    clientOptions
  );

  client.start();
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
