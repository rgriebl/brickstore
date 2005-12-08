#!/usr/bin/perl

undef $/;

$url = "http://peeron.com/inv/colors.txt";

open ( INPUT, "wget -UMozilla/5.0 -O- -q $url 2>/dev/null |" )
	or die "Couldn't download $url.";

print "struct color_peeron2bl { const char *m_name; int m_color; };\n";
print "\n";
print "static color_peeron2bl colortable [] = {\n";

while ( <INPUT> ) {
	while ( m:&limit=color2part\">([^<]+)</a></td><td>[0-9]*</td><td>[^<]*</td><td>([0-9]*)</td>:gs ) {
		print "\t{ \"$1\", ", ( $2 == "" ? -1 : $2 ), " },\n";
	}
}

print "\n";
print "\t{ 0, 0 }\n";
print "};\n";

close ( INPUT );
