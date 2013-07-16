function WebSocketTest() {
    if ("WebSocket" in window) {
	var url = "ws://example.com/service";
	var ws = new WebSocket(url);
	ws.onopen = function() {
            // Web Socket is connected. You can send data by send()
            // method
            ws.send("message to send");
	};
	ws.onmessage = function (evt) {
	    var received_msg = evt.data;
	    alert("recv:\r\n" + received_msg);
	};
	ws.onclose = function() { alert("websocket is closed."); };
	alert("WebSocket supported here!   :)\r\n\r\n" +
	      "Browser: " + navigator.userAgent + "\r\n\r\n" + 
	      "test by jimbergman.net (based on Google sample code)");
    } else {
	// the browser doesn't support WebSocket
	alert("WebSocket NOT supported here!   :(\r\n\r\n" + 
	      "Browser: " + navigator.userAgent + "\r\n\r\n" + 
	      "test by jimbergman.net (based on Google sample code)");
    }
}
