#!/usr/bin/env python3
# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

"""Smoke test for the BrickStore MCP server.

Drives a running BrickStore instance through all MCP tools via HTTP/SSE and
checks the responses. Expects a BrickStore instance with all MCP permissions
enabled (see mcp-test-sandbox.sh for a way to launch one without touching your
real configuration).

Usage: mcp-smoke-test.py [--host 127.0.0.1] [--port 45111] [--workdir DIR]
"""

import argparse
import http.client
import json
import os
import sys
import tempfile
import time


class McpClient:
    """Base client: JSON-RPC framing shared by both transports."""

    def __init__(self, host, port, timeout=60):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.next_id = 1

    def request(self, method, params=None, notification=False):
        rpc = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            rpc["params"] = params
        if not notification:
            rpc["id"] = self.next_id
            self.next_id += 1

        result = self._send(rpc, notification)
        if notification:
            return None
        if "error" in result:
            raise RuntimeError(f"JSON-RPC error: {result['error']}")
        return result["result"]

    def _send(self, rpc, notification):
        raise NotImplementedError

    def call(self, tool, arguments=None):
        # A transient modal dialog (e.g. the update-check popup at startup) blocks
        # mutating tools; that is expected, so retry for a short while.
        deadline = time.monotonic() + 30
        while True:
            result = self.request("tools/call", {"name": tool, "arguments": arguments or {}})
            is_error = result.get("isError", False)
            texts = [c["text"] for c in result.get("content", []) if c.get("type") == "text"]
            text = "\n".join(texts)
            if is_error and "waiting for user input" in text and time.monotonic() < deadline:
                time.sleep(1)
                continue
            break
        if is_error:
            return None, text
        try:
            return json.loads(text), None
        except json.JSONDecodeError:
            return text, None


class SseClient(McpClient):
    """Legacy HTTP+SSE transport (MCP 2024-11-05)."""

    def __init__(self, host, port, timeout=60):
        super().__init__(host, port, timeout)
        self.sse = http.client.HTTPConnection(host, port, timeout=timeout)
        self.sse.request("GET", "/sse", headers={"Accept": "text/event-stream"})
        self.sse_response = self.sse.getresponse()
        if self.sse_response.status != 200:
            raise RuntimeError(f"SSE connect failed: {self.sse_response.status}")

        event, data = self._read_event()
        if event != "endpoint":
            raise RuntimeError(f"expected endpoint event, got {event}")
        self.post_path = data

    def _read_event(self):
        event, data = None, []
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            line = self.sse_response.readline().decode("utf-8").rstrip("\n")
            if line.startswith("event: "):
                event = line[7:]
            elif line.startswith("data: "):
                data.append(line[6:])
            elif not line and event:
                return event, "\n".join(data)
        raise TimeoutError("timeout waiting for SSE event")

    def _send(self, rpc, notification):
        post = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        post.request("POST", self.post_path, body=json.dumps(rpc),
                     headers={"Content-Type": "application/json"})
        response = post.getresponse()
        response.read()
        post.close()
        if response.status != 202:
            raise RuntimeError(f"POST failed: {response.status}")
        if notification:
            return None

        while True:
            event, data = self._read_event()
            if event != "message":
                continue
            msg = json.loads(data)
            if msg.get("id") == rpc["id"]:
                return msg


class StreamableHttpClient(McpClient):
    """Streamable HTTP transport (MCP 2025-03-26): response in the POST body."""

    def _send(self, rpc, notification):
        post = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        post.request("POST", "/", body=json.dumps(rpc),
                     headers={"Content-Type": "application/json",
                              "Accept": "application/json"})
        response = post.getresponse()
        body = response.read()
        post.close()
        if notification:
            if response.status not in (200, 202):
                raise RuntimeError(f"POST failed: {response.status}")
            return None
        if response.status != 200:
            raise RuntimeError(f"POST failed: {response.status}")
        return json.loads(body)


passed, failed, skipped = [], [], []

def check(name, condition, detail=""):
    if condition:
        passed.append(name)
        print(f"  PASS {name}")
    else:
        failed.append(name)
        print(f"  FAIL {name}  {detail}")

def skip(name, detail=""):
    skipped.append(name)
    print(f"  SKIP {name}  {detail}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=45111)
    ap.add_argument("--workdir", default=None,
                    help="directory for test files (default: a fresh temp dir)")
    ap.add_argument("--transport", choices=["http", "sse"], default="http",
                    help="MCP transport to test: http = Streamable HTTP (default), "
                         "sse = legacy HTTP+SSE")
    args = ap.parse_args()

    workdir = args.workdir or tempfile.mkdtemp(prefix="brickstore-mcp-test-")
    os.makedirs(workdir, exist_ok=True)
    print(f"MCP smoke test against {args.host}:{args.port} "
          f"({args.transport} transport), workdir {workdir}")

    protocol = "2025-03-26" if args.transport == "http" else "2024-11-05"
    client_class = StreamableHttpClient if args.transport == "http" else SseClient
    c = client_class(args.host, args.port)

    # --- protocol ---------------------------------------------------------

    init = c.request("initialize", {"protocolVersion": protocol,
                                    "capabilities": {},
                                    "clientInfo": {"name": "mcp-smoke-test", "version": "1.0"}})
    check("initialize", init.get("serverInfo", {}).get("name") == "BrickStore MCP Server"
          and init.get("protocolVersion") == protocol)
    c.request("initialized", notification=True)
    check("ping", c.request("ping") == {})

    tools = {t["name"] for t in c.request("tools/list")["tools"]}
    expected = {
        "catalog_query", "catalog_schema", "catalog_price_guide", "catalog_picture",
        "document_list", "document_read",
        "document_add_lots", "document_edit_lots", "document_remove_lots",
        "document_create", "document_open", "document_import_bl_xml",
        "document_import_ldraw", "document_import_part_inventory",
        "document_save", "document_export_bl_xml",
    }
    check("tools/list", tools == expected, f"got {sorted(tools)}")

    # --- catalog (CatalogRead) --------------------------------------------

    r, err = c.call("catalog_query", {"item_id": "3001", "item_type": "Part"})
    check("catalog_query", r and r["total_count"] >= 1
          and any(i["id"] == "3001" for i in r["items"]), err or "")

    r, err = c.call("catalog_schema")
    check("catalog_schema", r and "Red" in r.get("colors", []), err or "")

    # Batched price guide: two valid parts (fetched from BrickLink in one batch if
    # not cached) plus one deliberately bad item, to exercise per-item errors.
    r, err = c.call("catalog_price_guide", {"items": [
        {"item_id": "3001", "item_type": "Part", "color": "Red"},
        {"item_id": "3003", "item_type": "Part", "color": "Blue"},
        {"item_id": "no-such-item", "item_type": "Part", "color": "Red"},
    ]})
    results = r["results"] if r else []
    # The two valid parts are either priced or "still fetching"; the bad one errors.
    valid_ok = all(("error" in res and "still fetching" in res["error"])
                   or ("current" in res and "prices" in res["current"]["new"])
                   for res in results[:2])
    check("catalog_price_guide (batch)", r and len(results) == 3
          and valid_ok
          and "error" in results[2] and "No item" in results[2]["error"], err or "")

    # Color is required for parts, reported per-item.
    r, err = c.call("catalog_price_guide", {"items": [
        {"item_id": "3001", "item_type": "Part"}]})
    check("catalog_price_guide (missing color)", r and len(r["results"]) == 1
          and "requires a color" in r["results"][0].get("error", ""), err or "")

    # Color is ignored for sets.
    r, err = c.call("catalog_price_guide", {"items": [
        {"item_id": "8880-1", "item_type": "Set"}]})
    res0 = r["results"][0] if r else {}
    check("catalog_price_guide (set, no color)", r and ("current" in res0
          or "still fetching" in res0.get("error", "")), err or "")

    # Image results carry an image content block, so inspect the raw result.
    img = c.request("tools/call", {"name": "catalog_picture",
                                   "arguments": {"item_id": "3001", "item_type": "Part",
                                                 "color": "Red"}})
    blocks = img.get("content", [])
    img_text = " ".join(b.get("text", "") for b in blocks)
    if img.get("isError") and "not available yet" in img_text:
        skip("catalog_picture", img_text)
    else:
        check("catalog_picture", not img.get("isError")
              and any(b.get("type") == "image" and b.get("mimeType") == "image/png"
                      and b.get("data") for b in blocks), str(blocks)[:200])

    # --- create / add / read / edit / remove (DocumentOpen + Edit) ---------

    r, err = c.call("document_create")
    check("document_create", r is not None, err or "")
    doc = r["document"]["index"]

    r, err = c.call("document_add_lots", {"index": doc, "lots": [
        {"item_id": "3001", "color": "Red", "quantity": 10, "price": 0.15},
        {"item_id": "3003", "item_type": "P", "color": "Black", "quantity": 5,
         "condition": "used", "remarks": "second lot"},
    ]})
    check("document_add_lots", r and len(r["added_lots"]) == 2
          and r["document"]["lot_count"] == 2, err or "")
    rows = [lot["row"] for lot in r["added_lots"]] if r else [0, 1]

    r, err = c.call("document_read", {"index": doc})
    lot0 = r["lots"][0] if r and r["lots"] else {}
    check("document_read", r and len(r["lots"]) == 2
          and lot0.get("item_id") == "3001" and lot0.get("color") == "Red"
          and lot0.get("quantity") == 10 and lot0.get("price") == 0.15, err or "")

    r, err = c.call("document_edit_lots", {"index": doc, "edits": [
        {"row": rows[0], "price": 0.25, "remarks": "mcp-test", "condition": "used"},
    ]})
    edited = r["edited_lots"][0] if r else {}
    check("document_edit_lots", r and edited.get("price") == 0.25
          and edited.get("remarks") == "mcp-test"
          and edited.get("condition") == "used", err or "")

    r, err = c.call("document_edit_lots", {"index": doc,
                                           "edits": [{"row": 9999, "price": 1}]})
    check("document_edit_lots (bad row)", r is None and "Invalid row" in (err or ""), err or "")

    r, err = c.call("document_edit_lots", {"index": doc,
                                           "edits": [{"row": rows[0], "color": "NoSuchColor"}]})
    check("document_edit_lots (bad color)", r is None and "Unknown color" in (err or ""), err or "")

    r, err = c.call("document_remove_lots", {"index": doc, "rows": [rows[1]]})
    check("document_remove_lots", r and len(r["removed_lots"]) == 1
          and r["document"]["lot_count"] == 1, err or "")

    # --- save / export / import round trips (DocumentSave + Open) ----------

    bsx = os.path.join(workdir, "mcp-test")
    r, err = c.call("document_save", {"index": doc, "file_path": bsx})
    check("document_save", r and r["document"]["file_path"].endswith(".bsx")
          and os.path.exists(bsx + ".bsx"), err or "")

    xml = os.path.join(workdir, "mcp-test.xml")
    r, err = c.call("document_export_bl_xml", {"index": doc, "file_path": xml})
    check("document_export_bl_xml", r and r["exported_lots"] == 1
          and os.path.exists(xml) and "3001" in open(xml).read(), err or "")

    r, err = c.call("document_import_bl_xml", {"file_path": xml})
    check("document_import_bl_xml", r and r["document"]["lot_count"] == 1, err or "")

    r, err = c.call("document_open", {"file_path": bsx + ".bsx"})
    check("document_open (already open)", r and r.get("already_open") is True, err or "")

    ldr = os.path.join(workdir, "mcp-test.ldr")
    with open(ldr, "w") as f:
        f.write("0 mcp test model\n"
                "1 4 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\n"
                "1 0 0 0 0 1 0 0 0 1 0 0 0 1 3003.dat\n")
    r, err = c.call("document_import_ldraw", {"file_path": ldr})
    check("document_import_ldraw", r and r["document"]["lot_count"] == 2, err or "")

    r, err = c.call("document_import_part_inventory", {"item_id": "8880-1"})
    if r is None and "not found" in (err or ""):
        skip("document_import_part_inventory", err)
    else:
        check("document_import_part_inventory",
              r and r["document"]["lot_count"] > 10, err or "")

    # --- negative tests -----------------------------------------------------

    r, err = c.call("document_read", {"index": 9999})
    check("document_read (bad index)", r is None and "Invalid document index" in (err or ""),
          err or "")

    r, err = c.call("document_create")
    r, err = c.call("document_save", {"index": r["document"]["index"]})
    check("document_save (never saved)", r is None
          and "not been saved" in (err or ""), err or "")

    r, err = c.call("document_open", {"file_path": os.path.join(workdir, "missing.bsx")})
    check("document_open (missing file)", r is None, err or "")

    r, err = c.call("document_add_lots", {"index": doc, "lots": [
        {"item_id": "no-such-item-id"}]})
    check("document_add_lots (bad item)", r is None and "No item with ID" in (err or ""), err or "")

    # --- summary ------------------------------------------------------------

    print(f"\n{len(passed)} passed, {len(failed)} failed, {len(skipped)} skipped")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
