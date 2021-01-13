import BrickStore 1.0
import QtQuick 2.12

Script {
    name: "Standard Print Script"
    author: "Robert Griebl"
    version: "0.1"
    type: Script.PrintingScript

    PrintingScriptTemplate {
        text: "Standard Print Template"
        printFunction: function(job, items) {
            xprint(job, items)
        }
    }

    function xprint(job, items)
    {
        if (!items.length)
            return;

        let ps = {};
        ps.x = 20.0;
        ps.y = 10.0;
        ps.w = job.paperSize.width - (2 * ps.x);
        ps.h = job.paperSize.height - (2 * ps.y);
        ps.pos = 0;

        let pagecount = 0;
        let itemh = 15;
        let pageh = 10;
        let reporth = 7.5;
        let listh = 7.5;
        let items_left = items.length;
        let itemflip = false;
        let rfooter = false;
        let page;

        let pagestat = new Array();

        //debugger

        while (items_left || !rfooter) {
            page = job.addPage()

            let fnt = Qt.font({ pointSize: 10 })
            page.font = fnt

            ps.pos = 0;
            pageHeader(page, ps);

            pagestat.lots = 0;
            pagestat.items = 0;
            pagestat.total = 0;

            if (job.pageCount == 1)
                reportHeader(page, ps);

            if (items_left)
                listHeader(page, ps);

            let items_on_page = false;

            while (items_left) {
                if (ps.pos >(ps.h - itemh - listh - pageh))
                    break;

                let item = items [items.length - items_left];
                listItem(page, ps, item, itemflip);

                pagestat.lots++;
                pagestat.items += item.quantity;
                pagestat.total += item.total;

                itemflip = itemflip ? false : true;
                items_left--;
                items_on_page = true;
            }

            if (items_on_page) {
                listFooter(page, ps, pagestat);
            }

            if (!items_left && !rfooter && (ps.pos <= (ps.h - reporth - pageh))) {
                reportFooter(page, ps);
                rfooter = true;
            }
            pageFooter(page, ps);
        }
    }



    function xs(pw, x)
    {
        return x * pw / 170.0;
    }

    function pageHeader(page, ps)
    {
        let oldfont = page.font;

        let y = ps.y + ps.pos;
        let h = 10;

        let f1 = Qt.font({ family: "Arial", pointSize: 12, bold: true })

        page.font = f1;
        page.drawText(ps.x, y, ps.w / 2, h, Page.AlignLeft | Page.AlignVCenter, "BrickStore");

        let f2 = Qt.font({ family: "Arial", pointSize: 12 })

        page.font = f2;
        let d = new Date();
        page.drawText(ps.x + ps.w / 2, y, ps.w / 2, h, Page.AlignRight | Page.AlignVCenter, Qt.formatDate(d));

        page.font = oldfont;

        ps.pos = h;
    }

    function pageFooter(page, ps)
    {
        let oldfont = page.font;

        let y = ps.y + ps.h - 10;
        let h = 10;

        let f3 = Qt.font({ family: "Arial", pointSize: 12, italic: true })

        page.font = f3;
//        page.drawText(ps.x             , y, ps.w * .75, h, Page.AlignLeft  | Page.AlignVCenter, Document.fileName);
        page.drawText(ps.x + ps.w * .75, y, ps.w * .25, h, Page.AlignRight | Page.AlignVCenter, "Page " + (page.number + 1));

        page.font = oldfont;

        ps.pos = y + h;
    }

    function reportHeader(page, ps)
    {
    }

    function reportFooter(page, ps)
    {
        let oldfont = page.font;
        let oldbg = page.backgroundColor;
        let oldfg = page.color;

        let y = ps.y + ps.pos;
        let h = 7.5;

        page.backgroundColor = "#333333"
        page.color = page.backgroundColor;
        page.drawRect(ps.x, y, ps.w, h);

        page.color = "#ffffff"
//        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10)    , h, Page.AlignHCenter | Page.AlignVCenter, Document.statistics.lots);
//        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10)    , h, Page.AlignHCenter | Page.AlignVCenter, Document.statistics.items);
//        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 40) - 2, h, Page.AlignRight   | Page.AlignVCenter, Money.toLocalString(Document.statistics.value, true));

        let fb = oldfont;
        fb.bold = true;
        page.font = fb;
        page.drawText(ps.x + 2, y, xs(ps.w, 100), h, Page.AlignLeft | Page.AlignVCenter, "Grand Total");

        page.color = oldfg;
        page.backgroundColor = oldbg;
        page.font = oldfont;

        ps.pos += h;
    }

    function listHeader(page, ps)
    {
        let oldbg = page.backgroundColor;
        let oldfg = page.color;

        let y = ps.y + ps.pos;
        let h = 7.5;

        page.backgroundColor = "#666666";
        page.color = page.backgroundColor;
        page.drawRect(ps.x, y, ps.w, h);

        page.color = "#ffffff";
        page.drawText(ps.x                , y, xs(ps.w, 20), h, Page.AlignCenter, "Image");
        page.drawText(ps.x + xs(ps.w,  20), y, xs(ps.w, 15), h, Page.AlignCenter, "Cond.");
        page.drawText(ps.x + xs(ps.w,  35), y, xs(ps.w, 75), h, Page.AlignCenter, "Part");
        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10), h, Page.AlignCenter, "Lots");
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h, Page.AlignCenter, "Qty");
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 20), h, Page.AlignCenter, "Price");
        page.drawText(ps.x + xs(ps.w, 150), y, xs(ps.w, 20), h, Page.AlignCenter, "Total");

        page.color = oldfg;
        page.backgroundColor = oldbg;

        ps.pos += h;
    }

    function listFooter(page, ps, pagestat)
    {
        let oldfont = page.font;
        let oldbg = page.backgroundColor;
        let oldfg = page.color;

        let y = ps.y + ps.pos;
        let h = 7.5;

        page.backgroundColor = "#666666"
        page.color = page.backgroundColor;
        page.drawRect(ps.x, y, ps.w, h);

        page.color = "#ffffff"
        page.drawText(ps.x + xs(ps.w, 110), y, xs(ps.w, 10),     h, Page.AlignHCenter | Page.AlignVCenter, pagestat.lots);
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10),     h, Page.AlignHCenter | Page.AlignVCenter, pagestat.items);
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 40) - 2, h, Page.AlignRight   | Page.AlignVCenter, pagestat.total.toLocaleCurrencyString(Qt.locale(), "BAR"));

        let fb = oldfont;
        fb.bold = true;
        page.font = fb;
        page.drawText(ps.x + 2, y, xs(ps.w, 100), h, Page.AlignLeft | Page.AlignVCenter, "Total");

        page.color = oldfg;
        page.backgroundColor = oldbg;
        page.font = oldfont;

        ps.pos += h;
    }

    function listItem(page, ps, item, odd)
    {
        let oldbg = page.backgroundColor;
        let oldfg = page.color;

        let y = ps.y + ps.pos;
        let h = 15;

        page.backgroundColor = odd ? "#dddddd" : "#ffffff"
        page.color = page.backgroundColor;
        page.drawRect(ps.x, y, ps.w, h);

        page.color = "#000000"
        page.drawImage(ps.x + 2, y, xs(ps.w, 15), h, item.image);

        page.drawText(ps.x + xs(ps.w,  20), y, xs(ps.w, 15), h, Page.AlignHCenter | Page.AlignVCenter, item.condition.used ? "Used" : "New");
        page.drawText(ps.x + xs(ps.w,  35), y, xs(ps.w, 85), h, Page.AlignLeft    | Page.AlignVCenter | Page.TextWordWrap, item.color.name + " " + item.name + " [" + item.id + "]");
        page.drawText(ps.x + xs(ps.w, 120), y, xs(ps.w, 10), h, Page.AlignHCenter | Page.AlignVCenter, item.quantity);
        page.drawText(ps.x + xs(ps.w, 130), y, xs(ps.w, 18), h, Page.AlignRight   | Page.AlignVCenter, item.price.toLocaleCurrencyString(Qt.locale(), "BAR"));
        page.drawText(ps.x + xs(ps.w, 149), y, xs(ps.w, 20), h, Page.AlignRight   | Page.AlignVCenter, item.total.toLocaleCurrencyString(Qt.locale(), "BAR"));

        page.color = oldfg;
        page.backgroundColor = oldbg;

        ps.pos += h;
    }

}

