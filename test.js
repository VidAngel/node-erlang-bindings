const {ErlangNode,atom} = require('./index');
const enode = new ErlangNode("mynodename", "abc123", "napi@127.0.0.1", function(response){
  console.log("RESPONSE", response);
});
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},65000)
console.log(enode.ex.Node.list(atom("hidden")));
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},30000)
setTimeout(function(){
  console.log(enode.ex.Node.list(atom("hidden")));
},500)
