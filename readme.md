Rabbit 3000 weather logger
==========

Some code I wrote a long time ago to read weather data from a Davis Vantage Pro
weather station (www.davisnet.com/weather). The weather station was connected to
a Rabbit Semiconductor embedded computer (an RCM3360) via a serial port.

The code periodically pulled data from the Vantage Pro's archive and wrote it to a
MySQL database. It also broadcast the LOOP command's current conditions via UDP on port
22554.

The code requires an external MySQL library available here: 
http://rabbitlib.cvs.sourceforge.net/viewvc/rabbitlib/db-mysql/mysql.lib?view=markup