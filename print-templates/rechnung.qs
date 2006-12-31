function load ( )
{
	var res = new Array;
	res.name = "Rechnung";
	return res;
}

function print ( )
{
	if ( !Document. items. length )
		return;

	var ps = new Array ( );
	ps. x = 20.0;
	ps. y = 10.0;
	ps. w = Job. paperSize. width - ( 2 * ps. x );
	ps. h = Job. paperSize. height - ( 2 * ps. y );
	ps. pos = 0;

        if ( !displayDialog ( ps ))
            return;

        //MessageBox.warning ( ps. address, MessageBox.Ok );

	var pagecount = 0;
	var itemh = 8;
	var pagehh = 72;
	var pagefh = 12;
	var reporthh = 0;
	var reportfh = 32;
	var listhh = 8;
	var listfh = 0;
	var items_left = Document. items. length;
	var itemflip = false;
	var rfooter = false;
	var page;

	var pagestat = new Array ( );

	while ( items_left || !rfooter ) {
		page = Job. addPage ( );

		var fnt = new Font ( );
		fnt. family = "Arial";
		fnt. pointSize = 9;
		page. font = fnt;

		ps. pos = 0;
		pageHeader ( page, ps );

		pagestat. lots = 0;
		pagestat. items = 0;
		pagestat. total = 0;

		if ( Job. pageCount == 1 ) {
			reportHeader ( page, ps );
		}

		if ( items_left ) {
			listHeader ( page, ps );
		}

		var items_on_page = false;

		while ( items_left ) {
			if ( ps. pos > ( ps. h - itemh - listfh - pagefh ))
				break;

			var item = Document. items [Document. items. length - items_left];

			listItem ( page, ps, item, itemflip );

			pagestat. lots++;
			pagestat. items += item. quantity;
			pagestat. total = pagestat. total + item. total;

			itemflip = itemflip ? false : true;
			items_left--;
			items_on_page = true;
		}

		if ( items_on_page ) {
			listFooter ( page, ps, pagestat );
		}

		if ( !items_left && !rfooter && ( ps. pos <= ( ps. h - reportfh - pagefh ))) {
			reportFooter ( page, ps );
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
	var oldfont = page. font;

        var oldcol = page. color;
        var oldbg = page. backgroundColor;

        var c1 = new Color ( "#26a" );
        var steps = 40;

        for ( var i = 0; i < steps; i++ ) {
            var c = new Color ( );
            c. setRgb ( c1. red + i * ( 255 - c1. red ) / steps,
                        c1. green + i * ( 255 - c1. green ) / steps,
                        c1. blue + i * ( 255 - c1. blue ) / steps );

            page. color = c;
            page. backgroundColor = c;
            page. drawRect ( 0, i, Job. paperSize. width, 1 );
        }

        page. color = oldcol;
        page. backgroundColor = oldbg;

	var f = page. font;

        f. family = "Times";
      	f. pointSize = 20;
	f. bold = true;
	f. italic = true;
	page. font = f;

	var ttitle = "BrickForge\nLEGO Online Shop auf BrickLink";

	page. drawText ( ps. x, ps. y, ps. w, 30, page.AlignCenter, ttitle );

        page. font = oldfont;
        f = page. font;
	f. pointSize = 7;
	f. bold = false;
	f. italic = true;
	page. font = f;

        var tsend = "S. Griebl, Drosselweg 4, 81827 München";
        page. drawText ( ps. x, ps. y + 35, 105, 8, page.AlignLeft | page.AlignTop, tsend );

	f. pointSize = 12;
	f. italic = false;
	f. bold = true;
	page. font = f;

	var tarr = [ "S. Griebl", "Drosselweg 4", "81827 München", "info@brickforge.de" ];

        for ( var i = 0; i < tarr. length; i++ ) {
            page. drawText ( ps. x + 110, ps. y + 35 + i * 6, ps. w - 110, 6, page.AlignLeft | page.AlignTop, tarr [i]);
        }

	f. pointSize = 10;
	f. bold = false;
	page. font = f;

        tarr = [ Utility. localDateString ( ps. date ), "Rechnung Nr. " + ps. renr ];

        for ( var i = 0; i < tarr. length; i++ ) {
            page. drawText ( ps. x + 110, ps. y + 63 + i * 6, ps. w - 110, 8, page.AlignLeft | page.AlignTop, tarr [i]);
        }

	f. pointSize = 12;
	f. bold = false;
	page. font = f;

        if ( page. number == 0 ) {
             page. drawText ( ps. x, ps. y + 40, 85, 35, page.AlignLeft | page.AlignTop, ps. address );
        }
        else {
             page. drawText ( ps. x, ps. y + 40, 85, 35, page.AlignLeft | page.AlignTop, "\n  Seite " + ( page. number + 1 ));
        }

	f. pointSize = 12;
	f. italic = false;
	f. bold = true;
	f. underline = true;
	page. font = f;

        page. drawText ( ps. x, ps. y + 75, 85, 7, page.AlignLeft | page.AlignTop, "Rechnung" );

	page. font = oldfont;

	ps. pos = ps. y + 72;
}

function pageFooter ( page, ps )
{
	var oldfont = page. font;
	var oldcol = page. color;

	var y = ps. y + ps. h - 12;
	var h = 12;

	var f = page. font;
	f. pointSize = 7;
        page. font = f;

        page. color = new Color ( "#333" );

        page. drawLine ( ps. x, y + 1, ps. x + ps. w, y + 1 );

        var tarr = [
                "Sandra Griebl",
                "Dresdner Bank, FFM",

                "Kto.Nr.: 86 43 131 00",
                "BLZ: 500 800 00",

                "IBAN: DE54 500800000864313100",
                "Swift-BIC: DRESDEFF",

                "Steuernummer",
                "185/388/13083"
        ];

        var tcol = tarr. length / 2;
        var tw = ps. w / tcol;

        for ( var i = 0; i < tcol; i++ ) {
            var ralign = ( i == ( tcol - 1 ));

            page. drawText ( ps. x + i * tw, y + 2, tw - 1, 5, ( ralign ? page.AlignRight : page.AlignLeft ) | page.AlignVCenter, tarr [2 * i] );
            page. drawText ( ps. x + i * tw, y + 7, tw - 1, 5, ( ralign ? page.AlignRight : page.AlignLeft ) | page.AlignVCenter, tarr [2 * i + 1] );
        }

	page. font = oldfont;
	ps. pos = y + h;
}

function reportHeader ( page, ps )
{
}

function reportFooter ( page, ps )
{
	var y = ps. y + ps. pos;
	var h = 8;

	page. drawText ( ps. x, y    , xs( ps. w, 145 ), h, page.AlignRight   | page.AlignVCenter, "Summe in $" );
	page. drawText ( ps. x + xs( ps. w, 149 ), y    , xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toString ( Document. statistics. value, true, 3 ));
	page. drawText ( ps. x, y + h, xs( ps. w, 145 ), h, page.AlignRight   | page.AlignVCenter, "Summe in " + Money. localCurrencySymbol ( ));

	page. drawText ( ps. x + xs( ps. w, 149 ), y + h, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( Document. statistics. value, true, 2 ));

	page. drawText ( ps. x, y + 2 * h, xs( ps. w, 145 ), h, page.AlignRight   | page.AlignVCenter, "Versandkosten" );
	page. drawText ( ps. x + xs( ps. w, 149 ), y + 2 * h, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( ps. pap, true, 2 ));

        var tot = ps. pap + Document. statistics. value;
	page. drawText ( ps. x, y + 3 * h, xs( ps. w, 145 ), h, page.AlignRight   | page.AlignVCenter, "Endbetrag" );
	page. drawText ( ps. x + xs( ps. w, 149 ), y + 3 * h, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( tot, true, 2 ));

        if ( ps. vat ) {
                var vat_amount = tot * ps. vat / ( 100.0 + ps. vat );
	        page. drawText ( ps. x, y + 2.5 * h, xs( ps. w, 145 ), h, page.AlignLeft   | page.AlignVCenter, "Im Endbetrag sind" );
         	page. drawText ( ps. x, y + 3 * h, xs( ps. w, 145 ), h, page.AlignLeft   | page.AlignVCenter,  ps. vat + "% MwSt = " + Money. toLocalString ( vat_amount, true, 2 ) + " enthalten." );
        }

	page. drawLine ( ps. x + xs( ps. w, 150 ), y, ps. x + xs( ps. w, 150 ), y + 4 * h );
	page. drawLine ( ps. x + xs( ps. w, 170 ), y, ps. x + xs( ps. w, 170 ), y + 4 * h );

	page. drawLine ( ps. x,                    y        , ps. x + xs( ps. w, 170 ), y );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y + 0.25 , ps. x + xs( ps. w, 170 ), y + 0.25 );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y + h    , ps. x + xs( ps. w, 170 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y + 3 * h, ps. x + xs( ps. w, 170 ), y + 3 * h );

        var oldlw = page. lineWidth;
        page. lineWidth = 0.5;

	page. drawLine ( ps. x + xs( ps. w, 150 ), y + 2 * h - 0.25, ps. x + xs( ps. w, 170 ), y + 2 * h - 0.25 );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y + 4 * h - 0.25, ps. x + xs( ps. w, 170 ), y + 4 * h - 0.25 );

        page. lineWidth = oldlw;

	ps. pos += h;
}

function listHeader ( page, ps )
{
	var oldbg = page. backgroundColor;
	var oldfg = page. color;
	var oldfnt = page. font;

	var y = ps. y + ps. pos;
	var h = 8;

        var fnt = page. font;
        fnt. bold = true;
        page. font = fnt;

	page. backgroundColor = new Color ( "#bbb" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#000" );
	page. drawText ( ps. x                   , y, xs( ps. w, 80 ), h, page.AlignCenter, "Artikel" );
	page. drawText ( ps. x + xs( ps. w,  80 ), y, xs( ps. w, 30 ), h, page.AlignCenter, "Farbe" );
	page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w,  8 ), h, page.AlignCenter, "Zust." );
	page. drawText ( ps. x + xs( ps. w, 118 ), y, xs( ps. w, 12 ), h, page.AlignCenter, "Anz." );
	page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 20 ), h, page.AlignCenter, "Einzel" );
	page. drawText ( ps. x + xs( ps. w, 150 ), y, xs( ps. w, 20 ), h, page.AlignCenter, "Gesamt" );

	page. drawLine ( ps. x + xs( ps. w,   0 ), y, ps. x + xs( ps. w,   0 ), y + h );
	page. drawLine ( ps. x + xs( ps. w,  80 ), y, ps. x + xs( ps. w,  80 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 110 ), y, ps. x + xs( ps. w, 110 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 118 ), y, ps. x + xs( ps. w, 118 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 130 ), y, ps. x + xs( ps. w, 130 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y, ps. x + xs( ps. w, 150 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 170 ), y, ps. x + xs( ps. w, 170 ), y + h );

        page. drawLine ( ps. x, y, ps. x + ps. w, y );

        var oldlw = page. lineWidth;
        page. lineWidth = 0.5;
        page. drawLine ( ps. x, y + h - 0.25, ps. x + ps. w, y + h - 0.25 );

        page. lineWidth = oldlw;
	page. color = oldfg;
	page. backgroundColor = oldbg;
	page. font = oldfnt;

	ps. pos += h;
}

function listFooter ( page, ps, pagestat )
{
}

function listItem ( page, ps, item, odd )
{
	var oldbg = page. backgroundColor;
	var oldfg = page. color;

	var y = ps. y + ps. pos;
	var h = 8;

	page. backgroundColor = new Color ( odd ? "#eee" : "#fff" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#000" );

	page. drawPixmap ( ps. x + 1, y, xs( ps. w, 8 ), 8, item. picture );
	page. drawText ( ps. x + xs( ps. w,  10 ), y, xs( ps. w, 69 ), h, page.AlignLeft    | page.AlignVCenter, item. id + " " + item. name );

	page. drawPixmap ( ps. x + xs( ps. w, 81 ), y, xs( ps. w, 4 ), 8, item. color. picture );
        page. drawText ( ps. x + xs( ps. w,  86 ), y, xs( ps. w, 23 ), h, page.AlignLeft    | page.AlignVCenter, item. color. name );

	page. drawText ( ps. x + xs( ps. w, 111 ), y, xs( ps. w,  6 ), h, page.AlignHCenter | page.AlignVCenter, item. condition. used ? "U" : "N" );
	page. drawText ( ps. x + xs( ps. w, 117 ), y, xs( ps. w, 12 ), h, page.AlignRight   | page.AlignVCenter, item. quantity );
	page. drawText ( ps. x + xs( ps. w, 129 ), y, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toString ( item. price, true, 3 ));
	page. drawText ( ps. x + xs( ps. w, 149 ), y, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toString ( item. total, true, 3 ));

	page. drawLine ( ps. x + xs( ps. w,   0 ), y, ps. x + xs( ps. w,   0 ), y + h );
	page. drawLine ( ps. x + xs( ps. w,  80 ), y, ps. x + xs( ps. w,  80 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 110 ), y, ps. x + xs( ps. w, 110 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 118 ), y, ps. x + xs( ps. w, 118 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 130 ), y, ps. x + xs( ps. w, 130 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 150 ), y, ps. x + xs( ps. w, 150 ), y + h );
	page. drawLine ( ps. x + xs( ps. w, 170 ), y, ps. x + xs( ps. w, 170 ), y + h );

        page. color = oldfg;
	page. backgroundColor = oldbg;

	ps. pos += h;
}


function displayDialog ( ps )
{
    var d = new Dialog;
    d. caption = "Rechnung";

    var address = new TextEdit;
    address. label = "Anschrift";
    d. add ( address );

    var renr = new LineEdit;
    renr. label = "Rechnungsnr.";
    renr. text = "1/2006";
    d. add ( renr );

    var date = new DateEdit;
    date. date = new Date;
    date. label = "Datum";
    d. add ( date );

    var vat = new NumberEdit;
    vat. decimals = 1;
    vat. label = "MwSt in %";
    vat. value = 16;
    d. add ( vat );

    var pap = new NumberEdit;
    pap. decimals = 2;
    pap. label = "Versand in EUR";
    pap. value = 1.70;
    d. add ( pap );

    if ( d. exec ( )) {
        ps. vat = vat. value;
        ps. pap = Money. fromLocalValue ( pap. value );
        ps. date = date. date;
        ps. renr = renr. text;
        ps. address = address. text;
        
        return true;
    }
    return false;
}
