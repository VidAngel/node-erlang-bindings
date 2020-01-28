const {ErlangNode,atom} = require('./index');
const enode = new ErlangNode("mynodename", "abc123", "napi@127.0.0.1");
console.log(enode.connect());
enode.rpc("Elixir.IO", "inspect", "dog");
console.log(enode.ex.GenServer.call(atom("SomeFakeProcessName"), atom("fakeaction")));
