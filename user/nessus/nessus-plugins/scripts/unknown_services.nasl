#
# This script was written by Michel Arboi <arboi@alussinan.org>
#
# It is released under the GNU Public Licence
#

if(description)
{
 script_id(11154);
 script_version ("$Revision: 1.16 $");
 
 name["english"] = "Unknown services banners";
 name["francais"] = "Banni�res des services inconnus";
 script_name(english:name["english"], francais:name["francais"]);
 
 desc["english"] = "
This plugin prints the banners from unknown service so that
the Nessus team can take them into account.

Risk factor : None";


 desc["francais"] = "
Ce plugin affiche les banni�res des services inconnus de fa�on �
ce que l'�quipe Nessus puisse en tenir compte.

Facteur de risque : Aucun";

 script_description(english:desc["english"], francais:desc["francais"]);
 
 summary["english"] = "Displays the unknown services banners";
 summary["francais"] = "Affiche les banni�res des services inconnus";
 script_summary(english:summary["english"], francais:summary["francais"]);
 
 script_category(ACT_END); 
 script_copyright(english:"This script is Copyright (C) 2002 Michel Arboi",
		francais:"Ce script est Copyright (C) 2002 Michel Arboi");
 family["english"] = "Misc.";
 family["francais"] = "Divers";

 script_family(english:family["english"], francais:family["francais"]);
 script_dependencie(
   "PC_anywhere_tcp.nasl",
   "SHN_discard.nasl",
   "X.nasl",
   "apcnisd_detect.nasl",
   "alcatel_backdoor_switch.nasl",
   "asip-status.nasl",
   "auth_enabled.nasl",
   "bugbear.nasl",
   "cifs445.nasl",
   "cp-firewall-auth.nasl",
   "dcetest.nasl",
   "dns_server.nasl",
   "echo.nasl",
   "find_service.nes",
   "find_service2.nasl",
   "mldonkey_telnet.nasl",
   "mssqlserver_detect.nasl",
   "mysql_version.nasl",
   "nessus_detect.nasl",
   "qmtp_detect.nasl",
   "radmin_detect.nasl",
   "rpc_portmap.nasl",
   "rpcinfo.nasl",
   "rsh.nasl",
   "telnet.nasl",
   "xtel_detect.nasl",
   "xtelw_detect.nasl");
 script_require_ports("Services/unknown");
 exit(0);
}

#
include("misc_func.inc");
include("dump.inc");

port = get_kb_item("Services/unknown");
if (! port) exit(0);
if (! get_port_state(port)) exit(0);
if (known_service(port: port)) exit(0);
if (port == 139) exit(0);	# Avoid silly messages

banner = get_unknown_banner(port: port, dontfetch: 1);

if (!banner) exit(0);

h = hexdump(ddata: banner);
if( strlen(banner) >= 3 )
{
m = string("An unknown server is running on this port.\n",
  "If you know what it is, please send this banner to the Nessus team:\n",
  h);
security_note(port: port, data: m);
}

