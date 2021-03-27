import BrickStore 1.0
import QtQuick 2.12

Script {
    name: "Print script (Comments, no price)"
    author: "GreatBrickLab"
    version: "0.2"
    type: Script.ExtensionScript

    PrintingScriptAction {
        text: "Print: Comments, no price"
        printFunction: printJob
    }

    function printJob(job, doc, items)
    {
        if (!items.length)
            return

        let ps = {};
        ps.doc = doc
        ps.ccode = BrickStore.symbolForCurrencyCode(doc.currencyCode)
        ps. x = 20.0;
        ps. y = 10.0;
        ps. w = job. paperSize.width - ( 2 * ps. x );
        ps. h = job. paperSize.height - ( 2 * ps. y );
        ps. pos = 0;

        let pagecount = 0;
        let itemh = 15;
        let pageh = 10;
        let reporth = 7.5;
        let listh = 7.5;
        let items_left = items. length;
        let itemflip = false;
        let rfooter = false;
        let page;

        let pagestat = {};
        let jobstat = { }

        jobstat.lots = 0
        jobstat.items = 0
        jobstat.total = 0

        while ( items_left || !rfooter ) {
            page = job. addPage ( );

            ps. pos = 0;
            pageHeader ( page, ps );

            pagestat. lots = 0;
            pagestat. items = 0;
            pagestat. total = 0;

            if ( job. pageCount == 1 ) {
                reportHeader ( page, ps );
            }

            if ( items_left ) {
                listHeader ( page, ps );
            }

            let items_on_page = false;

            while ( items_left ) {
                if ( ps. pos > ( ps. h - itemh - listh - pageh ))
                    break;

                let item = items [items. length - items_left];

                listItem ( page, ps, item, itemflip );

                pagestat. lots++;
                pagestat. items += item. quantity;
                pagestat. total += item. total;

                jobstat.lots++
                jobstat.items += item.quantity
                jobstat.total += item.total

                itemflip = itemflip ? false : true;
                items_left--;
                items_on_page = true;
            }

            if ( items_on_page ) {
                listFooter ( page, ps, pagestat );
            }

            if ( !items_left && !rfooter && ( ps. pos <= ( ps. h - reporth - pageh ))) {
                reportFooter ( page, ps, jobstat );
                rfooter = true;
            }

            pageFooter ( page, ps );
        }
    }

    function xs ( pw, x )
    {
        return x * pw / 170.0;
    }

    function pageHeader ( page, ps )
    {
        let y = ps. y + ps. pos;
        let h = 10;

        let f1 = Qt.font({ family: "Arial", pointSize: 12, bold: true })
        page. font = f1;
        page. drawText ( ps. x, y, ps. w / 2, h, Page.AlignLeft | Page.AlignVCenter,  "BrickStore" );

        let f2 = Qt.font({ family: "Arial", pointSize: 12 })
        page. font = f2;
        let d = new Date ( );
        page. drawText ( ps. x + ps. w / 2, y, ps. w / 2, h, Page.AlignRight | Page.AlignVCenter, Qt.formatDate(d));

        ps. pos = h;
    }

    function pageFooter ( page, ps )
    {
        let y = ps. y + ps. h - 10;
        let h = 10;

        let f3 = Qt.font({ family: "Arial", pointSize: 12, italic: true })
        page. font = f3;
        page. drawText ( ps. x              , y, ps. w * .75, h, Page.AlignLeft  | Page.AlignVCenter, ps.doc. fileName );
        page. drawText ( ps. x + ps. w * .75, y, ps. w * .25, h, Page.AlignRight | Page.AlignVCenter, "Page " + (page. number+1) );

        ps. pos = y + h;
    }

    function reportHeader ( page, ps )
    {
    }

    function reportFooter ( page, ps, jobstat )
    {
        let y = ps. y + ps. pos;
        let h = 7.5;

        let fnt = Qt.font({ family: "Arial", pointSize: 10 })
        page. font = fnt;

        page. backgroundColor = "#333333"
        page. color = page. backgroundColor;
        page. drawRect ( ps. x, y, ps. w, h );

        page. color = "white"
        page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 )    , h, Page.AlignHCenter | Page.AlignVCenter, jobstat. lots );
        page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 )    , h, Page.AlignHCenter | Page.AlignVCenter, jobstat. items );
        page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 40 ) - 2, h, Page.AlignRight   | Page.AlignVCenter, BrickStore.toCurrencyString( jobstat. total, ps.ccode));

        page.font. bold = true;
        page. drawText ( ps. x + 2, y, xs( ps. w, 100 ), h, Page.AlignLeft | Page.AlignVCenter, "Grand Total" );

        page. backgroundColor = "white"
        page. color = "black"

        ps. pos += h;
    }

    function listHeader ( page, ps )
    {
        let y = ps. y + ps. pos;
        let h = 7.5;

        let fnt = Qt.font({ family: "Arial", pointSize: 10 })
        page. font = fnt;

        page. backgroundColor = "#666666";
        page. color = page. backgroundColor;
        page. drawRect ( ps. x, y, ps. w, h );

        page. color = "white"
        page. drawText ( ps. x                   , y, xs( ps. w, 20 ), h, Page.AlignCenter, "Image" );
        page. drawText ( ps. x + xs( ps. w,  20 ), y, xs( ps. w, 15 ), h, Page.AlignCenter, "Cond." );
        page. drawText ( ps. x + xs( ps. w,  35 ), y, xs( ps. w, 75 ), h, Page.AlignCenter, "Part" );
        page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 ), h, Page.AlignCenter, "Lots" );
        page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ), h, Page.AlignCenter, "Qty" );
        page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 20 ), h, Page.AlignCenter, "Comments" );
        page. backgroundColor = "white"
        page. color = "black"

        ps. pos += h;
    }

    function listFooter ( page, ps, pagestat )
    {
        let y = ps. y + ps. pos;
        let h = 7.5;

        let fnt = Qt.font({ family: "Arial", pointSize: 10 })
        page. font = fnt;

        page. backgroundColor = "#666666";
        page. color = page. backgroundColor;
        page. drawRect ( ps. x, y, ps. w, h );

        page. color = "white";
        page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 ),     h, Page.AlignHCenter | Page.AlignVCenter, pagestat. lots );
        page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ),     h, Page.AlignHCenter | Page.AlignVCenter, pagestat. items );
        page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 40 ) - 2, h, Page.AlignRight   | Page.AlignVCenter, BrickStore.toCurrencyString( pagestat. total, ps.ccode ));

        page.font. bold = true;
        page. drawText ( ps. x + 2, y, xs( ps. w, 100 ), h, Page.AlignLeft | Page.AlignVCenter, "Total" );
        page.color = "black"
        page.backgroundColor = "white"

        ps. pos += h;
    }

    function listItem ( page, ps, item, odd )
    {
        let y = ps. y + ps. pos;
        let h = 15;

        let fnt = Qt.font({ family: "Arial", pointSize: 10 })
        page. font = fnt;

        page. backgroundColor = odd ? "#dddddd" : "#ffffff";
        page. color = page. backgroundColor;
        page. drawRect ( ps. x, y, ps. w, h );

        page. color = "black";
        page. backgroundColor = "white"
        page. drawImage ( ps. x + 2, y, xs( ps. w, 15 ), h, item. image );

        page. drawText ( ps. x + xs( ps. w,  20 ), y, xs( ps. w, 15 ), h, Page.AlignHCenter | Page.AlignVCenter, item. condition. used ? "Used" : "New" );
        page. drawText ( ps. x + xs( ps. w,  35 ), y, xs( ps. w, 85 ), h, Page.AlignLeft    | Page.AlignVCenter | Page.TextWordWrap, item. color. name + " " + item. name + " [" + item. id + "]" );
        page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ), h, Page.AlignHCenter | Page.AlignVCenter, item. quantity );
        page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 40 ), h, Page.AlignLeft   | Page.AlignVCenter, item. comments );

        ps. pos += h;
    }
}
