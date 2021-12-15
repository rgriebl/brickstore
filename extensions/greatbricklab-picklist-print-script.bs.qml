import BrickStore 1.0
import QtQuick 2.12

Script {
    name: "Picklist print script"
    author: "GreatBrickLab"
    version: "0.2"

    PrintingScriptAction {
        text: "Print: Picklist"
        printFunction: printJob
    }

    function printJob(job, doc, items)
    {
        if (!items.length)
            return

        let ps = {}
        ps.doc = doc
        ps.ccode = BrickStore.symbolForCurrencyCode(doc.currencyCode)
        ps.x = 4.0
        ps.y = 0.0
        ps.w = job.paperSize.width - (2 * ps.x)
        ps.h = job.paperSize.height - (2 * ps.y)
        ps.pos = 0

        let pagecount = 0
        let itemh = 15
        let pageh = 10
        let reporth = 7.5
        let listh = 7.5
        let items_left = items.length
        let itemflip = false
        let page

        let pagestat = {}
        let jobstat = { }

        jobstat.lots = 0
        jobstat.items = 0
        jobstat.total = 0

        while (items_left) {
            page = job.addPage()

            ps.pos = 0
            pageHeader(page, ps)

            pagestat.lots = 0
            pagestat.items = 0
            pagestat.total = 0

            if (items_left) {
                listHeader(page, ps)
            }

            let items_on_page = false

            while (items_left) {
                if (ps.pos > (ps.h - itemh - listh - pageh))
                    break

                let item = items [items.length - items_left]

                listItem(page, ps, item, itemflip)

                pagestat.lots++
                pagestat.items += item.quantity
                pagestat.total += item.total

                jobstat.lots++
                jobstat.items += item.quantity
                jobstat.total += item.total

                itemflip = itemflip ? false : true
                items_left--
                items_on_page = true
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

        let f1 = Qt.font({ family: "Arial", pointSize: 12, bold: true })
        page.font = f1
        page.drawText(ps.x, y, ps.w *.75, h, PrintPage.AlignLeft | PrintPage.AlignVCenter, ps.doc.fileName)

        let f2 = Qt.font({ family: "Arial", pointSize: 12 })
        page.font = f2
        let d = new Date()
        page.drawText(ps.x + ps.w / 2, y, ps.w / 2, h, PrintPage.AlignRight | PrintPage.AlignVCenter, Qt.formatDate(d))

        ps.pos = h
    }

    function pageFooter(page, ps)
    {
        let y = ps.y + ps.h - 10
        let h = 10

        let f3 = Qt.font({ family: "Arial", pointSize: 12, italic: true })
        page.font = f3
        page.drawText(ps.x              , y, ps.w * .75, h, PrintPage.AlignLeft  | PrintPage.AlignVCenter, ps.doc.fileName)
        page.drawText(ps.x + ps.w * .75, y, ps.w * .25, h, PrintPage.AlignRight | PrintPage.AlignVCenter, "Page " + (page.number+1))

        ps.pos = y + h
    }

    function listHeader(page, ps)
    {
        let y = ps.y + ps.pos
        let h = 7.5

        let fnt = Qt.font({ family: "Arial", pointSize: 10 })
        page.font = fnt

        page.backgroundColor = "#666666"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "white"
        page.drawText(ps.x                , y, xs(ps.w, 20), h, PrintPage.AlignCenter, "Price")
        page.drawText(ps.x + xs(ps.w,  20), y, xs(ps.w, 65), h, PrintPage.AlignCenter, "Color and Part")

        page.drawText(ps.x + xs(ps.w,  85), y, xs(ps.w, 20), h, PrintPage.AlignCenter, "BIN")
        page.drawText(ps.x + xs(ps.w, 105), y, xs(ps.w, 15), h, PrintPage.AlignCenter, "Image")
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 19), h, PrintPage.AlignCenter, "QTY     Color")
        page.drawText(ps.x + xs(ps.w, 142), y, xs(ps.w, 28), h, PrintPage.AlignCenter, "Drawer")
        page.backgroundColor = "white"
        page.color = "black"

        ps.pos += h
    }

    function listItem(page, ps, item, odd)
    {
        let y = ps.y + ps.pos
        let h = 14

        let fnt = Qt.font({ family: "Arial", pointSize: 9 })
        page.font = fnt

        page.backgroundColor = odd ? "#dddddd" : "#ffffff"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "black"
        page.backgroundColor = "white"


        page.drawText(ps.x +            2, y, xs(ps.w, 16), h, PrintPage.AlignHCenter | PrintPage.AlignVCenter, ps.ccode + BrickStore.toCurrencyString(item.price))

        page.drawText(ps.x + xs(ps.w, 20), y, xs(ps.w, 65), h, PrintPage.AlignLeft    | PrintPage.AlignVCenter | PrintPage.TextWordWrap, item.color.name + " " + item.name + " [" + item.id + "]")

        page.font = Qt.font({ family: "Times New Roman", pointSize: 22 })

        page.drawText(ps.x + xs(ps.w, 87), y, xs(ps.w, 16), h, PrintPage.AlignLeft  | PrintPage.AlignVCenter | PrintPage.TextWordWrap, item.comments)

        page.drawImage(ps.x + xs(ps.w, 105), y, xs(ps.w, 15), h, item.image)

        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 15), h, PrintPage.AlignLeft | PrintPage.AlignVCenter, "x" + item.quantity)

        page.drawImage(ps.x + xs(ps.w, 135), y, xs(ps.w, 5), h, item.color.image)

        page.font = Qt.font({ family: "Arial", pointSize: 14 })
        page.drawText(ps.x + xs(ps.w, 142), y, xs(ps.w, 28), h, PrintPage.AlignLeft | PrintPage.AlignVCenter | PrintPage.TextWordWrap, item.remarks)


        page.font = fnt
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 35), h, PrintPage.AlignHLeft | PrintPage.TextWordWrap, item.color.name)

        ps.pos += h
    }
}
