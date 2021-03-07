import BrickStore 1.0
import QtQuick 2.12

Script {
    name: "Standard Print Script"
    author: "Robert Griebl"
    version: "0.2"
    type: Script.PrintingScript

    PrintingScriptTemplate {
        text: "Standard Print Template"
        printFunction: function(job, doc, lots) {
            printJob(job, doc, lots)
        }
    }

    function printJob(job, doc, lots)
    {
        if (!lots.length)
            return

        let ps = {}
        ps.doc = doc
        ps.ccode = BrickStore.symbolForCurrencyCode(doc.currencyCode)
        ps.x = 20.0
        ps.y = 10.0
        ps.w = job.paperSize.width - (2 * ps.x)
        ps.h = job.paperSize.height - (2 * ps.y)
        ps.pos = 0

        let pagecount = 0
        let rowh = 15
        let pageh = 10
        let reporth = 7.5
        let listh = 7.5
        let lots_left = lots.length
        let alternate = false
        let rfooter = false
        let page

        let pagestat = { }
        let jobstat = { }

        jobstat.lots = 0
        jobstat.items = 0
        jobstat.total = 0

        while (lots_left || !rfooter) {
            page = job.addPage()

            ps.pos = 0
            pageHeader(page, ps)

            pagestat.lots = 0
            pagestat.items = 0
            pagestat.total = 0

            if (job.pageCount == 1)
                reportHeader(page, ps)

            if (lots_left)
                listHeader(page, ps)

            let lots_on_page = false

            while (lots_left) {
                if (ps.pos >(ps.h - rowh - listh - pageh))
                    break

                let lot = lots [lots.length - lots_left]
                listLot(page, ps, lot, alternate)

                pagestat.lots++
                pagestat.items += lot.quantity
                pagestat.total += lot.total

                jobstat.lots++
                jobstat.items += lot.quantity
                jobstat.total += lot.total

                alternate = !alternate
                lots_left--
                lots_on_page = true
            }

            if (lots_on_page) {
                listFooter(page, ps, pagestat)
            }

            if (!lots_left && !rfooter && (ps.pos <= (ps.h - reporth - pageh))) {
                reportFooter(page, ps, jobstat)
                rfooter = true
            }
            pageFooter(page, ps)
        }
    }

    function xs(pw, x)
    {
        return x * pw / 170.0
    }

    function pageHeader(page, ps)
    {
        let y = ps.y + ps.pos
        let h = 10

        page.color = "black"
        page.backgroundColor = "white"

        page.font = Qt.font({ family: "Arial", pointSize: 12, bold: true })
        page.drawText(ps.x, y, ps.w / 2, h, Page.AlignLeft | Page.AlignVCenter, "BrickStore")

        //page.font = Qt.font({ family: "Arial", pointSize: 12 })
        page.font.bold = false
        page.drawText(ps.x + ps.w / 2, y, ps.w / 2, h, Page.AlignRight | Page.AlignVCenter,
                      Qt.formatDate(new Date()))

        ps.pos = h
    }

    function pageFooter(page, ps)
    {
        let y = ps.y + ps.h - 10
        let h = 10

        page.color = "black"
        page.backgroundColor = "white"
        page.font = Qt.font({ family: "Arial", pointSize: 12, italic: true })
        page.drawText(ps.x             , y, ps.w * .75, h,
                      Page.AlignLeft  | Page.AlignVCenter,
                      ps.doc.fileName)
        page.drawText(ps.x + ps.w * .75, y, ps.w * .25, h,
                      Page.AlignRight | Page.AlignVCenter,
                      "Page " + (page.number + 1))

        ps.pos = y + h
    }

    function reportHeader(page, ps)
    {
    }

    function reportFooter(page, ps, jobstat)
    {
        let y = ps.y + ps.pos
        let h = 7.5

        page.backgroundColor = "#333333"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "white"
        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10), h,
                      Page.AlignRight | Page.AlignVCenter,
                      jobstat.lots)
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h,
                      Page.AlignRight | Page.AlignVCenter,
                      jobstat.items)
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 39), h,
                      Page.AlignRight | Page.AlignVCenter,
                      BrickStore.toCurrencyString(jobstat.total, ps.ccode))

        page.font.bold = true
        page.drawText(ps.x + 2, y, xs(ps.w, 100), h, Page.AlignLeft | Page.AlignVCenter,
                      "Grand Total")

        ps.pos += h
    }

    function listHeader(page, ps)
    {
        let y = ps.y + ps.pos
        let h = 7.5

        page.backgroundColor = "#666666"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "white"
        page.font = Qt.font({ family: "Arial", pointSize: 10 })

        page.drawText(ps.x                , y, xs(ps.w, 20), h, Page.AlignCenter, "Image")
        page.drawText(ps.x + xs(ps.w,  20), y, xs(ps.w, 15), h, Page.AlignCenter, "Cond.")
        page.drawText(ps.x + xs(ps.w,  35), y, xs(ps.w, 75), h, Page.AlignVCenter | Page.AlignLeft,   "Part")
        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10), h, Page.AlignVCenter | Page.AlignRight,  "Lots")
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h, Page.AlignVCenter | Page.AlignRight,  "Qty")
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 19), h, Page.AlignVCenter | Page.AlignRight,  "Price")
        page.drawText(ps.x + xs(ps.w, 150), y, xs(ps.w, 19), h, Page.AlignVCenter | Page.AlignRight,  "Total")

        ps.pos += h
    }

    function listFooter(page, ps, pagestat)
    {
        let y = ps.y + ps.pos
        let h = 7.5

        page.backgroundColor = "#666666"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "white"
        page.font = Qt.font({ family: "Arial", pointSize: 10 })

        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10), h,
                      Page.AlignRight | Page.AlignVCenter,
                      pagestat.lots)
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h,
                      Page.AlignRight | Page.AlignVCenter,
                      pagestat.items)
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 39), h,
                      Page.AlignRight | Page.AlignVCenter,
                      BrickStore.toCurrencyString(pagestat.total, ps.ccode))

        page.font.bold = true
        page.drawText(ps.x + 2, y, xs(ps.w, 100), h,
                      Page.AlignLeft | Page.AlignVCenter,
                      "Total")

        ps.pos += h
    }

    function listLot(page, ps, lot, alternate)
    {
        let y = ps.y + ps.pos
        let h = 15

        page.backgroundColor = alternate ? "#dddddd" : "white"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "black"
        page.font = Qt.font({ family: "Arial", pointSize: 10 })

        page.drawImage(ps.x + 2, y, xs(ps.w, 15), h, lot.image)

        page.drawText(ps.x + xs(ps.w,  20), y, xs(ps.w, 15), h,
                      Page.AlignHCenter | Page.AlignVCenter,
                      lot.condition == BrickLink.Used ? "Used" : "New")
        page.drawText(ps.x + xs(ps.w,  35), y, xs(ps.w, 85), h,
                      Page.AlignLeft | Page.AlignVCenter | Page.TextWordWrap,
                      lot.color.name + " " + lot.name + " [" + lot.id + "]")
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h,
                      Page.AlignRight | Page.AlignVCenter,
                      lot.quantity)
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 19), h,
                      Page.AlignRight | Page.AlignVCenter,
                      BrickStore.toCurrencyString(lot.price, ps.ccode))
        page.drawText(ps.x + xs(ps.w, 150), y, xs(ps.w, 19), h,
                      Page.AlignRight | Page.AlignVCenter,
                      BrickStore.toCurrencyString(lot.total, ps.ccode))

        ps.pos += h
    }

}
