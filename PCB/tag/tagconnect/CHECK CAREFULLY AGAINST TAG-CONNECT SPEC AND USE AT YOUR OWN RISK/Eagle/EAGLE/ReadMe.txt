
This CadSoft EAGLE file has been submitted by Jiri Rehorek of ROFA, Prague.

June 24, 2009 	- Modified script so that the paste mask layer doesn't allow paste on the Tag-Connect pads.
		- Added solder mask to the bottom traces on the TC2030-MCP-NL-CP package so that copper is exposed and can be easily soldered to when securing pins.

July 15, 2009	- Changed Pin alignment drill size on the TC2030-MCP and TC2030-MCP-NL to 0.0374" from 0.035" to account for manufacturing tolorence.

PLEASE CHECK YOUR PASTE-MASK LAYER TO ENSURE NO SOLDER PASTE iS DEPOISTED ON THE Tag-Connect pads!!!


Tag-connect.scr is the multi-purpose library part which is independent from the version of the PCB CAD. 
It can be used from the Eagle ver 3 up.
It can be imported into any user library.

 
How-to-import:
 
1) Start Eagle SW
2) Open desired library, where you want to import the part (for exapmle: "mylib.lbr") - FILE-OPEN-LIBRARY-mylib.lbr
    you can do it whenever you want,even if you have opened the schematics, or PCB
3) Take SCRIPT command from FILE menu and find the "Tag-connect.scr" file and choose it.
4) The part was imported into the "mylib.lbr"
5) Save the modified library.
6) Done
 
If you want to put the part into the new library (for example Tag-Connect.lbr) simply go to create new library fromcontrol panel and follow the above steps.
 

