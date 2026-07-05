// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

// Exports the current document as a CSV or JSON file that can be uploaded to LEGO's official
// "Pick a Brick" service. For each lot it resolves the LEGO element id (part-color-code) from
// BrickStore's catalog and then verifies against lego.com which of the possible element ids is
// actually orderable right now (a part+color can have several element ids over its lifetime,
// and only some remain available).
//
// Based on an idea from mgodineau: https://github.com/rgriebl/brickstore/pull/1041
//
// Requires BrickStore >= 1.1 (for Item.pccsForColor() and BrickStore.saveTextFileAs()).

import BrickStore 1.1
import BrickLink 1.1
import QtQuick

Script {
    name: "Lego Pick a Brick export"
    author: "Robert Griebl"
    version: "1.0"

    ExtensionScriptAction {
        text: qsTr("Export: Lego Pick a Brick CSV...")
        actionFunction: () => exportPickABrick("csv")
    }
    ExtensionScriptAction {
        text: qsTr("Export: Lego Pick a Brick JSON...")
        actionFunction: () => exportPickABrick("json")
    }

    // --- lego.com specifics (the fragile part - validate/adjust against the live site) ---------

    readonly property string pabUrl: "https://www.lego.com/pick-and-build/pick-a-brick"
    readonly property int    batchSize: 100   // == perPage; do not query more ids than one page holds

    // Returns true if the response page for a batch query mentions the given item (by its
    // BrickLink id or one of its alternate ids), i.e. the queried element id returned a result.
    function responseContainsItem(html, work) {
        if (html.indexOf(work.itemId) >= 0)
            return true
        for (let i = 0; i < work.altIds.length; ++i) {
            if (work.altIds[i].length && (html.indexOf(work.altIds[i]) >= 0))
                return true
        }
        return false
    }

    // --- element-id selection (offline, from the catalog) -------------------------------------

    // All element ids for item+color, ordered best-first: newest new-style codes first, then the
    // (at most one) old-style "part-number * 100 + color" code as a last resort.
    function orderedCandidates(item, color) {
        let pccs = Array.from(item.pccsForColor(color))   // copy the C++ sequence into a JS array
        let itemId = Number(item.id)   // NaN for non-numeric ids -> everything counts as new-style
        let isOld = (pcc) => (Math.floor(pcc / 100) === itemId)

        let news = pccs.filter(pcc => !isOld(pcc)).sort((a, b) => b - a)
        let olds = pccs.filter(isOld)
        return news.concat(olds)
    }

    // --- serialization ------------------------------------------------------------------------

    function toCsv(byElement) {
        let out = "elementId,quantity\n"
        for (let id in byElement)
            out += id + "," + byElement[id] + "\n"
        return out
    }

    function toJson(byElement) {
        let arr = []
        for (let id in byElement)
            arr.push({ elementId: String(id), quantity: String(byElement[id]) })
        return JSON.stringify(arr, null, 2)
    }

    // --- main ---------------------------------------------------------------------------------

    function exportPickABrick(format) {
        let doc = BrickStore.activeDocument
        if (!doc)
            return

        // export the selection if there is one, otherwise the whole document
        let lots = doc.selectedLots
        if (!lots.length) {
            lots = []
            for (let i = 0; i < doc.lotCount; ++i)
                lots.push(doc.lots.at(i))
        }

        let work = []          // { itemId, altIds, quantity, candidates: [best..worst] }
        let skippedNoCode = 0  // lots for which the catalog knows no element id at all
        for (let i = 0; i < lots.length; ++i) {
            let lot = lots[i]
            if (lot.item.isNull)
                continue
            let candidates = orderedCandidates(lot.item, lot.color)
            if (!candidates.length) {
                skippedNoCode++
                continue
            }
            work.push({ itemId: lot.item.id, altIds: lot.item.alternateIds,
                        quantity: lot.quantity, candidates: candidates, done: false, resultPcc: 0 })
        }

        if (!work.length) {
            console.log("Pick a Brick export: nothing to export (" + skippedNoCode + " lots have no element id)")
            return
        }

        let canceled = false
        let xhr = null
        let skippedUnavailable = 0

        doc.startBlockingOperation(qsTr("Exporting to Lego Pick a Brick..."), function () {
            canceled = true
            if (xhr)
                xhr.abort()
        })

        function finishAndSave() {
            doc.endBlockingOperation()

            let byElement = {}
            for (let i = 0; i < work.length; ++i) {
                let w = work[i]
                if (w.resultPcc)
                    byElement[w.resultPcc] = (byElement[w.resultPcc] || 0) + w.quantity
            }

            let count = Object.keys(byElement).length
            console.log("Pick a Brick export: " + count + " element(s), "
                        + (skippedNoCode + skippedUnavailable) + " lot(s) skipped")
            if (!count)
                return

            let base = (doc.title && doc.title.length) ? doc.title : "pick-a-brick"
            if (format === "csv") {
                BrickStore.saveTextFileAs(toCsv(byElement), base + ".csv",
                                          qsTr("Lego Pick a Brick CSV"), [ "csv" ])
            } else {
                BrickStore.saveTextFileAs(toJson(byElement), base + ".json",
                                          qsTr("Lego Pick a Brick JSON"), [ "json" ])
            }
        }

        function processNextBatch() {
            if (canceled) {
                doc.endBlockingOperation()
                return
            }

            // pick up to batchSize still-pending items, at most one per BrickLink item so a found
            // item id maps unambiguously to the single element id we queried for it
            let batch = []
            let seen = ({})
            for (let i = 0; i < work.length; ++i) {
                let w = work[i]
                if (w.done)
                    continue
                if (!w.candidates.length) {   // exhausted without a hit
                    w.done = true
                    skippedUnavailable++
                    continue
                }
                if (seen[w.itemId])
                    continue
                seen[w.itemId] = true
                batch.push(w)
                if (batch.length >= batchSize)
                    break
            }

            if (!batch.length) {
                finishAndSave()
                return
            }

            let done = work.length - work.filter(w => !w.done).length
            doc.blockingOperationTitle = qsTr("Checking Lego Pick a Brick... %1 / %2")
                                             .arg(done).arg(work.length)

            let query = batch.map(w => w.candidates[0]).join("+")
            xhr = new XMLHttpRequest()
            xhr.open("GET", pabUrl + "?perPage=" + batchSize + "&query=" + query)
            xhr.onreadystatechange = function () {
                if (xhr.readyState !== XMLHttpRequest.DONE)
                    return
                if (canceled) {
                    doc.endBlockingOperation()
                    return
                }
                if (xhr.status !== 200) {
                    doc.endBlockingOperation()
                    console.log("Pick a Brick export: request failed (HTTP " + xhr.status + ")")
                    return
                }

                let html = xhr.responseText
                for (let i = 0; i < batch.length; ++i) {
                    let w = batch[i]
                    if (responseContainsItem(html, w)) {
                        w.resultPcc = w.candidates[0]
                        w.done = true
                    } else {
                        w.candidates.shift()   // that element id is not available; try the next one
                    }
                }
                processNextBatch()
            }
            xhr.send()
        }

        processNextBatch()
    }
}
