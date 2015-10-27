      var currentTimeMs=(new Date()).getTime();
	  var websocket;
	  
	  var output;

		function searchKeyPress(e)
		{
		    // look for window.event in case event isn't passed in
		    e = e || window.event;
		    if (e.keyCode == 13)
		    {
		        document.getElementById('btn').click();
		        return false;
		    }
		    return true;
		}

	  function init()
	  {
		output = document.getElementById("output");
		testWebSocket();	
	  }
	  function testWebSocket()
	  {
	  	if (location.host != "")
	  	{
			var wsUri = "ws://" + location.host + "/";
			//var wsUri = "ws://192.168.1.24/";
			websocket = new WebSocket(wsUri);
			websocket.onopen = function(evt) { onOpen(evt) };
			websocket.onclose = function(evt) { onClose(evt) };
			websocket.onmessage = function(evt) { onMessage(evt) };
			websocket.onerror = function(evt) { onError(evt) };		
	  	}
	  }
	  function onOpen(evt)
	  {
		writeToScreen("CONNECTED");
		//setInterval(function() {
		//	doSend("GiveMeData");
		//}, 150);	
	  }
	  function onClose(evt)
	  {
		writeToScreen("DISCONNECTED");
	  }
	  function onMessage(evt)
	  {
		writeToScreen('<span style="color: blue;">Time: ' + ((new Date()).getTime()-currentTimeMs) +'ms Received: ' + evt.data+'</span>');
		document.getElementById("result").innerHTML = evt.data;
		//line1.append(new Date().getTime(), evt.data);
	  	currentTimeMs = (new Date()).getTime();
		//websocket.close();
	  }
	  function onError(evt)
	  {
		writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
	  }
	  function doSend(element)
	  {
		//writeToScreen("SENT: " + message); 
		textToSend = element.value;
		websocket.send(textToSend);
	  }
	  function doSendCommand(textToSend)
	  {
		if(websocket!=null)
			websocket.send(textToSend);
	  }
	  function writeToScreen(message)
	  {
		var pre = document.createElement("p");
		pre.style.wordWrap = "break-word";
		pre.innerHTML = message;
		output.appendChild(pre);
	  }
	  function doDisconnect()
	  {
		var disconnect = document.getElementById("disconnect");
		disconnect.disabled = true;
		websocket.close();
	  }

	function jogXYClick(a)
	{
		var bothMotorOption = document.getElementById("bothSingle");
		var coord = a.substring(0, 1);
		var sign = a.substring(1, 2);
		var value = a.substring(2, a.length);
		if (coord = 'X')
			var command = "X" + sign + value + " Y" + sign + value;
		else
			var command = "Z" + sign + value + " E" + sign + value;

		writeToScreen("SENT: " + command); 
		doSendCommand(command);		
	}

	function jogClick(a)
	{
		writeToScreen("SENT: " + a); 
		doSendCommand(a);		
	}

window.addEventListener("load", init, false);
