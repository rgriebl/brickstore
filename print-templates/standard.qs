function load ( )
{
	var res = new Array;
	res.name = "Standard";
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

	var pagecount = 0;
	var itemh = 15;
	var pageh = 10;
	var reporth = 7.5;
	var listh = 7.5;
	var items_left = Document. items. length;
	var itemflip = false;
	var rfooter = false;
	var page;

	var pagestat = new Array ( );

	while ( items_left || !rfooter ) {
		page = Job. addPage ( );

		var fnt = new Font ( );
		fnt. family = "Arial";
		fnt. pointSize = 10;
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
			if ( ps. pos > ( ps. h - itemh - listh - pageh ))
				break;
			
			var item = Document. items [Document. items. length - items_left];

			listItem ( page, ps, item, itemflip );

			pagestat. lots++;
			pagestat. items += item. quantity;
			pagestat. total += item. total;

			itemflip = itemflip ? false : true;
			items_left--;
			items_on_page = true;
		}

		if ( items_on_page ) {
			listFooter ( page, ps, pagestat );
		}

		if ( !items_left && !rfooter && ( ps. pos <= ( ps. h - reporth - pageh ))) {
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

	var y = ps. y + ps. pos;
	var h = 10;

	var f1 = new Font ( );
	f1. family = "Arial";
	f1. pointSize = 12;
	f1. bold = true;

	page. font = f1;
	page. drawText ( ps. x, y, ps. w / 2, h, page.AlignLeft | page.AlignVCenter,  "BrickStore" );

	var f2 = new Font ( );
	f2. family = "Arial";
	f2. pointSize = 12;

	page. font = f2;
	var d = new Date ( );
	page. drawText ( ps. x + ps. w / 2, y, ps. w / 2, h, page.AlignRight | page.AlignVCenter, Utility. localDateString ( d ));

	page. font = oldfont;

	ps. pos = h;
}

function pageFooter ( page, ps )
{
	var oldfont = page. font;

	var y = ps. y + ps. h - 10;
	var h = 10;

	var f3 = new Font ( );
	f3. family = "Arial";
	f3. pointSize = 12;
	f3. italic = true;

	page. font = f3;
	page. drawText ( ps. x              , y, ps. w * .75, h, page.AlignLeft  | page.AlignVCenter, Document. fileName );
	page. drawText ( ps. x + ps. w * .75, y, ps. w * .25, h, page.AlignRight | page.AlignVCenter, "Page " + (page. number+1) );

	page. font = oldfont;

	ps. pos = y + h;
}

function reportHeader ( page, ps )
{
}

function reportFooter ( page, ps )
{
	var oldfont = page. font;
	var oldbg = page. backgroundColor;
	var oldfg = page. color;

	var y = ps. y + ps. pos;
	var h = 7.5;

	page. backgroundColor = new Color ( "#333" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#fff" );
	page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 )    , h, page.AlignHCenter | page.AlignVCenter, Document. statistics. lots );
	page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 )    , h, page.AlignHCenter | page.AlignVCenter, Document. statistics. items );
	page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 40 ) - 2, h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( Document. statistics. value, true ));

	var fb = oldfont;
	fb. bold = true;
	page. font = fb;
	page. drawText ( ps. x + 2, y, xs( ps. w, 100 ), h, page.AlignLeft | page.AlignVCenter, "Grand Total" );

	page. color = oldfg;
	page. backgroundColor = oldbg;
	page. font = oldfont;

	ps. pos += h;
}

function listHeader ( page, ps )
{
	var oldbg = page. backgroundColor;
	var oldfg = page. color;

	var y = ps. y + ps. pos;
	var h = 7.5;

	page. backgroundColor = new Color ( "#666" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#fff" );
	page. drawText ( ps. x                   , y, xs( ps. w, 20 ), h, page.AlignCenter, "Image" );
	page. drawText ( ps. x + xs( ps. w,  20 ), y, xs( ps. w, 15 ), h, page.AlignCenter, "Cond." );
	page. drawText ( ps. x + xs( ps. w,  35 ), y, xs( ps. w, 75 ), h, page.AlignCenter, "Part" );
	page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 ), h, page.AlignCenter, "Lots" );
	page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ), h, page.AlignCenter, "Qty" );
	page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 20 ), h, page.AlignCenter, "Price" );
	page. drawText ( ps. x + xs( ps. w, 150 ), y, xs( ps. w, 20 ), h, page.AlignCenter, "Total" );

	page. color = oldfg;
	page. backgroundColor = oldbg;

	ps. pos += h;
}

function listFooter ( page, ps, pagestat )
{
	var oldfont = page. font;
	var oldbg = page. backgroundColor;
	var oldfg = page. color;

	var y = ps. y + ps. pos;
	var h = 7.5;

	page. backgroundColor = new Color ( "#666" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#fff" );
	page. drawText ( ps. x + xs( ps. w, 110 ), y, xs( ps. w, 10 ),     h, page.AlignHCenter | page.AlignVCenter, pagestat. lots );
	page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ),     h, page.AlignHCenter | page.AlignVCenter, pagestat. items );
	page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 40 ) - 2, h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( pagestat. total, true ));

	var fb = oldfont;
	fb. bold = true;
	page. font = fb;
	page. drawText ( ps. x + 2, y, xs( ps. w, 100 ), h, page.AlignLeft | page.AlignVCenter, "Total" );

	page. color = oldfg;
	page. backgroundColor = oldbg;
	page. font = oldfont;

	ps. pos += h;
}

function listItem ( page, ps, item, odd )
{
	var oldbg = page. backgroundColor;
	var oldfg = page. color;

	var y = ps. y + ps. pos;
	var h = 15;

	page. backgroundColor = new Color ( odd ? "#ddd" : "#fff" );
	page. color = page. backgroundColor;
	page. drawRect ( ps. x, y, ps. w, h );

	page. color = new Color ( "#000" );
	page. drawPixmap ( ps. x + 2, y, xs( ps. w, 15 ), h, item. picture );

	page. drawText ( ps. x + xs( ps. w,  20 ), y, xs( ps. w, 15 ), h, page.AlignHCenter | page.AlignVCenter, item. condition. used ? "Used" : "New" );
	page. drawText ( ps. x + xs( ps. w,  35 ), y, xs( ps. w, 85 ), h, page.AlignLeft    | page.AlignVCenter, item. color. name + " " + item. name + " [" + item. id + "]" );
	page. drawText ( ps. x + xs( ps. w, 120 ), y, xs( ps. w, 10 ), h, page.AlignHCenter | page.AlignVCenter, item. quantity );
	page. drawText ( ps. x + xs( ps. w, 130 ), y, xs( ps. w, 18 ), h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( item. price, true ));
	page. drawText ( ps. x + xs( ps. w, 149 ), y, xs( ps. w, 20 ), h, page.AlignRight   | page.AlignVCenter, Money. toLocalString ( item. total, true ));

	page. color = oldfg;
	page. backgroundColor = oldbg;

	ps. pos += h;
}
