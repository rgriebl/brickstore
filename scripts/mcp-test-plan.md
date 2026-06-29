# MCP Server Test Plan

## Transports

The server speaks two transports on the same port:

- **Streamable HTTP** (MCP 2025-03-26): a single `POST /` endpoint, the default
  for current clients. In a client config, use `"type": "http"` with
  `"url": "http://localhost:<port>"`.
- **Legacy HTTP+SSE** (MCP 2024-11-05): `GET /sse` opens the stream, `POST
  /message` carries the messages. Use `"type": "sse"` with
  `"url": "http://localhost:<port>/sse"`.

## Automated smoke test

`mcp-smoke-test.py` drives a running BrickStore instance through all MCP tools
and checks the responses: protocol handshake, tool registry, catalog queries,
batched price guide, picture (image content block), document
create/read/add/edit/remove, BSX save/open and BrickLink XML export/import
round trips, LDraw import, part-out, plus negative tests (bad indexes, rows,
colors, items, missing files). `--transport {http,sse}` selects the transport
(default: http).

The easiest way to run it is through the sandbox launcher, which cannot
interfere with a regular BrickStore installation (separate config, data and
autosave locations; only the BrickLink database and image cache are shared).
It runs the suite over both transports:

```sh
scripts/mcp-test-sandbox.sh            # builds/qt-dev-desktop-debug binary
BRICKSTORE_BIN=path/to/brickstore scripts/mcp-test-sandbox.sh
scripts/mcp-test-sandbox.sh --keep     # keep the sandbox dir for inspection
```

To run against a manually started instance instead (all MCP permissions must
be enabled in Settings > AI):

```sh
python3 scripts/mcp-smoke-test.py [--port 45111] [--transport http|sse]
```

## Manual tests

These cannot be covered by the headless smoke test:

1. **Modal-dialog gate**: open a document, then open any modal dialog (e.g.
   Edit > Price > Set...). While the dialog is up, call a mutating tool
   (`document_add_lots`). Expected: an error result "BrickStore is waiting for
   user input...". Close the dialog, retry: the call succeeds.

2. **Permission matrix**: toggle the individual checkboxes in Settings > AI
   and verify `tools/list` after each change: catalog tools only with catalog
   access; read/open/edit/save tool groups appear exactly when their checkbox
   is enabled; no server at all when "Enable MCP server" is off.
   Note: changing permissions (or the port) restarts the MCP server, so clients
   must reconnect afterwards.

3. **Port change and status indicator**: change the port in Settings > AI; the
   toolbar MCP icon and the AI-page status line must reflect the new port, and
   a client pointed at it must connect. With the server disabled, the toolbar
   icon is hidden.

4. **Undo semantics**: after an MCP `document_add_lots`/`edit_lots`/
   `remove_lots` call, a single Ctrl+Z in the UI must revert the entire call,
   and the view must update accordingly.

5. **Concurrent UI use**: with the document visible, run MCP edits and verify
   the view, totals and modification marker update live.

6. **Picture rendering**: call `catalog_picture` from a client that renders
   image content (e.g. VS Code) and confirm the item image is shown inline.
