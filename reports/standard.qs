function load ( )
{
	var res = new Array;

	res.name = "Standard";

	return res;

	System.print("load" );
}

function print ( items )
{
	var page = ReportPage.addPage();
	page.drawText ( 10, 10, 100, 50, "HelloWorld" );

	System.print("XXXX");
}
