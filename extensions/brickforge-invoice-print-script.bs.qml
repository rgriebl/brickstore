import BrickStore 1.0
import QtQuick 2.12

Script {
    name: "BrickForge invoice print script"
    author: "Robert Griebl"
    version: "0.8"

    PrintingScriptAction {
        text: "Drucken: Rechnung Brickforge"
        printFunction: printJob
    }

    function printJob(job, doc, lots)
    {
        if (!lots.length)
            return
//        console.log(doc.order)
//        console.log(JSON.stringify(doc))
//        console.log(doc.order.id)
//        console.log(JSON.stringify(doc.order))

        if (!doc.order || doc.order.id == "") {
            throw new Error("This print template only works on orders.")
        }
        if (doc.order.currencyCode != "EUR")
            throw new Error("This print template expects the order currency to be 'EUR'.")

        let ps = {}
        ps.doc = doc
        ps.order = doc.order
        ps.ccode = BrickStore.symbolForCurrencyCode(doc.currencyCode)
        ps.x = 20.0
        ps.y = 10.0
        ps.w = job.paperSize.width - (2 * ps.x)
        ps.h = job.paperSize.height - (2 * ps.y)

        for (let duplicate = 0; duplicate <= 1; ++duplicate) {
            ps.pos = 0

            let pagecount = 0
            let lineHeight = 6
            let reportFooterHeight = 4 * lineHeight
            let pageFooterHeight = 12
            let lotsLeft = lots.length
            let itemEvenOdd = false
            let reportFooterDone = false
            let pageNumber = 0
            let page

            let pagestat = { }
            let jobstat = { }

            jobstat.lots = 0
            jobstat.items = 0
            jobstat.total = 0


            while (lotsLeft || !reportFooterDone) {
                page = job.addPage()
                pageNumber++

                ps.pos = 0
                pageHeader(page, ps, pageNumber, duplicate)

                pagestat.lots = 0
                pagestat.items = 0
                pagestat.total = 0

                if (lotsLeft)
                    listHeader(page, ps)

                while (lotsLeft) {
                    if (ps.pos > (ps.h - pageFooterHeight - lineHeight))
                        break

                    let lot = lots [lots.length - lotsLeft]
                    listItem(page, ps, lot, itemEvenOdd)

                    pagestat.lots++
                    pagestat.items += lot.quantity
                    pagestat.total += lot.total

                    jobstat.lots++
                    jobstat.items += lot.quantity
                    jobstat.total += lot.total

                    itemEvenOdd = itemEvenOdd ? false : true
                    lotsLeft--
                }

                if (!lotsLeft && !reportFooterDone && (ps.pos <= (ps.h - pageFooterHeight - reportFooterHeight))) {
                    reportFooter(page, ps, jobstat)
                    reportFooterDone = true
                }
                pageFooter(page, ps)
            }
        }
    }

    function xs(pw, x)
    {
        return x * pw / 170.0
    }

    function pageHeader(page, ps, pageNumber, duplicate)
    {
//      Fancy gradient, but hard on the cyan toner
//        let c1 = Qt.rgba(.13, .4, .66, 1)
//        let steps = 40

//        for (let i = 0; i < steps; i++) {
//            let c = Qt.rgba(c1.r + i * (1 - c1.r) / steps,
//                            c1.g + i * (1 - c1.g) / steps,
//                            c1.b + i * (1 - c1.b) / steps, 1)

//            page.backgroundColor = c
//            page.color = page.backgroundColor
//            page.drawRect(0, i, ps.w + 2 * ps.x, 1)
//        }

        page.color = "black"
        page.backgroundColor = "white"

        page.font = Qt.font({ family: "Arial", pointSize: 20, bold: true })

        if (duplicate) {
            page.drawRect(10, 10, 40, 10)
            page.drawText(10, 10, 40, 10, PrintPage.AlignCenter, "DUPLIKAT")
        }

        let ttitle = "BrickForge\nLEGO Online Shop auf BrickLink"

        page.font.italic = true
        page.drawText(ps.x, ps.y, ps.w, 30, PrintPage.AlignCenter, ttitle)

        page.font.pointSize = 7
        page.font.bold = false
        page.font.italic = false

        let tsend = "S. Griebl, Herzog-Tassilo-Ring 52, 85604 Zorneding"
        page.drawText(ps.x, ps.y + 35, 105, 8, PrintPage.AlignLeft | PrintPage.AlignTop, tsend)

        page.font.pointSize = 12
        page.font.bold = true

        let tarr = [ "S. Griebl", "Herzog-Tassilo-Ring 52", "85604 Zorneding", "bricklink@brickforge.de" ]

        for (let i = 0; i < tarr.length; i++) {
            page.drawText(ps.x + 110, ps.y + 35 + i * 6, ps.w - 110, 6,
                          PrintPage.AlignLeft | PrintPage.AlignTop, tarr[i])
        }

        page.font.pointSize = 10
        page.font.bold = false

        tarr = [ Qt.formatDate(new Date()), "Rechnung Nr. " + ps.order.id ]

        for (let i = 0; i < tarr.length; i++) {
            page.drawText(ps.x + 110, ps.y + 63 + i * 6, ps.w - 110, 8,
                          PrintPage.AlignLeft | PrintPage.AlignTop, tarr[i])
        }

        page.font.pointSize = 12

        if (pageNumber == 1) {
            page.drawText(ps.x, ps.y + 40, 85, 35, PrintPage.AlignLeft | PrintPage.AlignTop, ps.order.address)
        } else {
            page.drawText(ps.x, ps.y + 40, 85, 35, PrintPage.AlignLeft | PrintPage.AlignTop,
                           "\n  Seite " + (pageNumber + 1))
        }

        page.font.bold = true
        page.font.underline = true

        page.drawText(ps.x, ps.y + 75, 85, 7, PrintPage.AlignLeft | PrintPage.AlignTop, "Rechnung")

        ps.pos = ps.y + 72

        page.color = "#666"
        page.drawLine(0, 87, 10, 87)
        page.drawLine(0, 192, 10, 192)
        page.color = "black"
    }

    function pageFooter(page, ps)
    {
        let y = ps.y + ps.h - 12
        let h = 12

        page.color = "#333"
        page.backgroundColor = "white"
        page.font = Qt.font({ family: "Times", pointSize: 7 })

        page.drawLine(ps.x, y + 1, ps.x + ps.w, y + 1)

        let tarr = [
                    "Sandra Griebl",
                    "Bank: comdirect",

                    "IBAN: DE79 200411440727112500",
                    "Swift-BIC: COBADEHD044",

                    "Steuernummer",
                    "185/388/13083",

                    "Leistungsdatum entspricht",
                    "Rechnungsdatum"
                ]

        let tcol = tarr.length / 2
        let tw = ps.w / tcol

        for (let i = 0; i < tcol; i++) {
            let ralign = (i >= (tcol / 2))

            page.drawText(ps.x + i * tw, y + 2, tw - 1, 5,
                          (ralign ? PrintPage.AlignRight : PrintPage.AlignLeft) | PrintPage.AlignVCenter,
                          tarr[2 * i])
            page.drawText(ps.x + i * tw, y + 7, tw - 1, 5,
                          (ralign ? PrintPage.AlignRight : PrintPage.AlignLeft) | PrintPage.AlignVCenter,
                          tarr[2 * i + 1])
        }
        ps.pos = y + h
    }

    function reportFooter(page, ps)
    {
        let y = ps.y + ps.pos
        let h = 6

        page.font = Qt.font({ family: "Arial", pointSize: 9 })

        page.drawText(ps.x, y, xs(ps.w, 145), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      "Summe")
        page.drawText(ps.x + xs(ps.w, 149), y, xs(ps.w,20), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(ps.order.orderTotal, ps.ccode, 2))

        page.drawText(ps.x, y + h, xs(ps.w, 145), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      "Versandkosten")
        page.drawText(ps.x + xs(ps.w, 149), y + h, xs(ps.w, 20), h,
                      PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(ps.order.shipping, ps.ccode, 2))

        page.drawText(ps.x, y + 2 * h, xs(ps.w, 145), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      "Gutschrift")
        page.drawText(ps.x + xs(ps.w, 149), y + 2 * h, xs(ps.w, 20), h,
                      PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(-ps.order.credit, ps.ccode, 2))

        // order.grandTotal can be off by 0.01
        let gtotal = ps.order.orderTotal + ps.order.shipping - ps.order.credit

        page.drawText(ps.x, y + 3 * h, xs(ps.w, 145), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      "Endbetrag")
        page.drawText(ps.x + xs(ps.w, 149), y + 3 * h, xs(ps.w, 20), h,
                      PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(gtotal, ps.ccode, 2))

        if (ps.order.paymentType.toLowerCase().includes("(onsite)")) {
            page.drawText(ps.x, y + 1 * h, xs(ps.w, 145), h, PrintPage.AlignLeft | PrintPage.AlignVCenter,
                          "Die Zahlung erfolgte bereits bei der Bestellung.")
        }

        let vatCharges = ps.order.vatCharges
        let vatLine1 = ""
        let vatLine2 = ""
        if (vatCharges) {
            let vat = Math.round(vatCharges / (gtotal - vatCharges) * 100)
            vatLine1 = "Im Endbetrag sind"
            vatLine2 = vat + "% MwSt = " + BrickStore.toCurrencyString(vatCharges, ps.ccode, 2)
                           + " enthalten."
        } else {
            vatLine1 = "Diese Rechnung ist umsatzsteuerbefreit"
            vatLine2 = "gemäß §19(1) UStG."
        }
        page.drawText(ps.x, y + 2.5 * h, xs(ps.w, 145), h, PrintPage.AlignLeft | PrintPage.AlignVCenter,
                      vatLine1)
        page.drawText(ps.x, y + 3 * h, xs(ps.w, 145), h, PrintPage.AlignLeft | PrintPage.AlignVCenter,
                      vatLine2)

        page.drawLine(ps.x + xs(ps.w, 150), y, ps.x + xs(ps.w, 150), y + 4 * h)
        page.drawLine(ps.x + xs(ps.w, 170), y, ps.x + xs(ps.w, 170), y + 4 * h)

        page.drawLine(ps.x,                 y        , ps.x + xs(ps.w, 170), y)
        page.drawLine(ps.x + xs(ps.w, 150), y + 0.25 , ps.x + xs(ps.w, 170), y + 0.25)
        page.drawLine(ps.x + xs(ps.w, 150), y + h    , ps.x + xs(ps.w, 170), y + h)
        page.drawLine(ps.x + xs(ps.w, 150), y + 3 * h, ps.x + xs(ps.w, 170), y + 3 * h)

        let oldlw = page.lineWidth
        page.lineWidth = 0.5

        page.drawLine(ps.x + xs(ps.w, 150), y + 1 * h - 0.25, ps.x + xs(ps.w, 170), y + 1 * h - 0.25)
        page.drawLine(ps.x + xs(ps.w, 150), y + 4 * h - 0.25, ps.x + xs(ps.w, 170), y + 4 * h - 0.25)

        page.lineWidth = oldlw

        ps.pos += 4 * h
    }

    function listHeader(page, ps)
    {
        let y = ps.y + ps.pos
        let h = 6

        page.backgroundColor = "#bbb"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "black"
        page.font = Qt.font({ family: "Arial", pointSize: 9 })

        page.drawText(ps.x  + xs(ps.w,  1), y, xs(ps.w, 79), h, PrintPage.AlignVCenter | PrintPage.AlignLeft, "Artikel")
        page.drawText(ps.x + xs(ps.w,  80), y, xs(ps.w, 30), h, PrintPage.AlignCenter, "Farbe")
        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w,  8), h, PrintPage.AlignCenter, "Zust")
        page.drawText(ps.x + xs(ps.w, 118), y, xs(ps.w, 11), h, PrintPage.AlignVCenter | PrintPage.AlignRight, "Anz.")
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 19), h, PrintPage.AlignVCenter | PrintPage.AlignRight, "Einzel")
        page.drawText(ps.x + xs(ps.w, 150), y, xs(ps.w, 19), h, PrintPage.AlignVCenter | PrintPage.AlignRight, "Gesamt")

        page.drawLine(ps.x + xs(ps.w,   0), y, ps.x + xs(ps.w,   0), y + h)
        page.drawLine(ps.x + xs(ps.w,  80), y, ps.x + xs(ps.w,  80), y + h)
        page.drawLine(ps.x + xs(ps.w, 110), y, ps.x + xs(ps.w, 110), y + h)
        page.drawLine(ps.x + xs(ps.w, 118), y, ps.x + xs(ps.w, 118), y + h)
        page.drawLine(ps.x + xs(ps.w, 130), y, ps.x + xs(ps.w, 130), y + h)
        page.drawLine(ps.x + xs(ps.w, 150), y, ps.x + xs(ps.w, 150), y + h)
        page.drawLine(ps.x + xs(ps.w, 170), y, ps.x + xs(ps.w, 170), y + h)

        page.drawLine(ps.x, y, ps.x + ps.w, y)

        let oldlw = page.lineWidth
        page.lineWidth = 0.5
        page.drawLine(ps.x, y + h - 0.25, ps.x + ps.w, y + h - 0.25)

        page.lineWidth = oldlw

        ps.pos += h
    }

    function listItem(page, ps, item, odd)
    {
        let y = ps.y + ps.pos
        let h = 6

        page.font = Qt.font({ family: "Arial", pointSize: 9 })

        page.backgroundColor = odd ? "#eee" : "#fff"
        page.color = page.backgroundColor
        page.drawRect(ps.x, y, ps.w, h)

        page.color = "black"

        page.drawImage(ps.x + 1, y, xs(ps.w, 6), 6, item.image)
        page.drawText(ps.x + xs(ps.w, 10), y, xs(ps.w, 69), h, PrintPage.AlignLeft | PrintPage.AlignVCenter,
                      item.id + " " + item.name)

        page.drawImage(ps.x + xs(ps.w, 81), y, xs(ps.w, 5), 6, item.color.image)
        page.drawText(ps.x + xs(ps.w, 86), y, xs(ps.w, 23), h, PrintPage.AlignLeft | PrintPage.AlignVCenter,
                      item.color.name)

        page.drawText(ps.x + xs(ps.w, 111), y, xs(ps.w, 6), h, PrintPage.AlignCenter,
                      item.condition == BrickLink.Condition.Used ? "U" : "N")
        page.drawText(ps.x + xs(ps.w, 117), y, xs(ps.w, 12), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      item.quantity)
        page.drawText(ps.x + xs(ps.w, 129), y, xs(ps.w, 20), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(item.price, ps.ccode, 3))
        page.drawText(ps.x + xs(ps.w, 149), y, xs(ps.w, 20), h, PrintPage.AlignRight | PrintPage.AlignVCenter,
                      BrickStore.toCurrencyString(item.total, ps.ccode, 3))

        page.drawLine(ps.x + xs(ps.w,   0), y, ps.x + xs(ps.w,   0), y + h)
        page.drawLine(ps.x + xs(ps.w,  80), y, ps.x + xs(ps.w,  80), y + h)
        page.drawLine(ps.x + xs(ps.w, 110), y, ps.x + xs(ps.w, 110), y + h)
        page.drawLine(ps.x + xs(ps.w, 118), y, ps.x + xs(ps.w, 118), y + h)
        page.drawLine(ps.x + xs(ps.w, 130), y, ps.x + xs(ps.w, 130), y + h)
        page.drawLine(ps.x + xs(ps.w, 150), y, ps.x + xs(ps.w, 150), y + h)
        page.drawLine(ps.x + xs(ps.w, 170), y, ps.x + xs(ps.w, 170), y + h)

        ps.pos += h
    }
}
