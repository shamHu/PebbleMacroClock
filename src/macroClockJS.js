function appMessageAck(e) {
	console.log("JSON options sent to Pebble");
}

function appMessageNack(e) {
	console.log("JSON options not sent to Pebble: " + e.error.message);
}

Pebble.addEventListener('showConfiguration', function(e) {
	var options = JSON.parse(window.localStorage.getItem('macroClockOptions'));
	var configLink = 'http://dustinhu.com/projects/library/MacroClock/Configuration.html';
	if (options !== null) {
		configLink += '?&backgroundColor=' + encodeURIComponent(options['backgroundColor']) +
			'&hourColor=' + encodeURIComponent(options['hourColor']) +
			'&handColor=' + encodeURIComponent(options['handColor']) +
			'&dotColor=' + encodeURIComponent(options['dotColor']) +
			'&handOutlineColor=' + encodeURIComponent(options['handOutlineColor']);
	}
	console.log("opening " + configLink);
	Pebble.openURL(configLink);
});

Pebble.addEventListener('webviewclosed', function(e) {
	console.log('Configuration window returned: ' + e.response);
	var options = JSON.parse(decodeURIComponent(e.response));
	console.log("Options = " + JSON.stringify(options));
	window.localStorage.setItem('macroClockOptions', JSON.stringify(options));
	Pebble.sendAppMessage(options, appMessageAck, appMessageNack);
});