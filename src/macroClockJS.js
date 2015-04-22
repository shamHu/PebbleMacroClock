

Pebble.addEventListener('showConfiguration', function(e) {
	console.log("event showConfiguration called");
	Pebble.openURL('http://dustinhu.com/projects/library/MacroClock/Configuration.html');
	console.log("opened");
});