const {ErlangNode,atom} = require('./index');
const enode = new ErlangNode("mynodename", "abc123", "napi@127.0.0.1");
enode.connect();
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},65000)
enode.receive(function(data) {
  console.log("Received", data);
});
console.log(enode.ex.Node.list(atom("hidden")));
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},30000)
