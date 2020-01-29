const {ErlangNode,atom} = require('./index');
const enode = new ErlangNode("mynodename", "abc123", "napi@127.0.0.1");

enode.on('message', function(response) {
  console.log("MESSAGE", response);
});

enode.on('tick', function() {
  console.log("TICK");
});

enode.on('error', function(errno) {
  console.log("ERROR", errno);
});

setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},65000);
console.log(enode.ex.Node.list(atom("hidden")));
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},30000)
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},500)
