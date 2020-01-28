const assert = require('assert');
const eqq = assert.strictEqual;
const {ErlangNode,tuple,charlist,atom,Tuple,Atom} = require('./index');

const spawn = require('child_process').spawn;

const proc = spawn('elixir', ['--no-halt','--name','napi@127.0.0.1','--cookie',
                              'abc123','-e','IO.puts :ready'])

let done = false;
proc.on('error', e => {throw new Error(e.toString())});
proc.stderr.on('data', data => console.log(data.toString()));
proc.on('close', _ => assert(done, "Process closed before completion"));

proc.stdout.on('data', data => {
  const str = data.toString().trim();
  if(str !== 'ready') return;
  try {
  const enode = new ErlangNode("mynodename", "abc123", "napi@127.0.0.1");
  enode.connect();
  eqq(enode.rpc("Elixir.Enum", "max", [5,4,9,2]), 9);
  eqq(enode.rpc("Elixir.Enum", "member?", [5,4,9,2], 2), true);
  eqq(enode.rpc("Elixir.Enum", "member?", [5,4,9,2], 242), false);
  eqq(enode.rpc("Elixir.Kernel", "+", 4, 5.9), 9.9);
  eqq(enode.rpc("Elixir.IO", "inspect", charlist("I am a charlist")), "I am a charlist");
  eqq(enode.rpc("Elixir.IO", "inspect", "Hello!"), "Hello!");
  eqq(enode.rpc("Elixir.IO", "inspect", "⋄~♥~⋄"), "⋄~♥~⋄");
  eqq(enode.rpc("Elixir.String", "upcase", "guy fieri"), "GUY FIERI");
  let tupe = enode.rpc("Elixir.IO", "inspect", tuple(1, 2, 3.506, atom("HEY"))).value;
  assert(tupe[3] instanceof Atom);
  eqq(tupe.length, 4);
  eqq(tupe[2],3.506);
  const arr = [1,2,3.5,"abc",[4,5,"heyyy"]];
  assert.deepEqual(enode.rpc("Elixir.IO", "inspect", arr), arr);
  tupe = enode.rpc("Elixir.IO", "inspect", tuple(1,2,3,['a','b','c',tuple("hello",atom("world"))]));
  eqq(tupe[3][2], 'c');
  eqq(tupe[3][3][0], 'hello');
  assert.deepEqual(enode.rpc("Elixir.IO","inspect", []), []);
  eqq(enode.rpc("Elixir.Enum", "at", [5], 10), null);
  eqq(enode.rpc("Elixir.Enum", "at", [5], 0), 5);
  eqq(enode.rpc("Elixir.Enum", "at", [], 10), null);
  assert.deepEqual(enode.rpc("Elixir.IO", "inspect", {x: 4, y: -24}), {x: 4, y: -24});
  assert.deepEqual(enode.rpc("Elixir.IO", "inspect", {}), {});

  eqq(enode.ex.List.first([6,3,2]), 6);
  eqq(enode.ex.Enum.at([4,5,2], 1), 5);
  eqq(enode.ex.String.pad_leading("a", 4, "0"), "000a");
  function badenode(){  enode.ex.GenServer.call(atom("SomeFakeProcessName"), atom("fakeaction"))};
  assert.throws(badenode, /badrpc\(noproc\)/);
  assert.throws(enode.ex.FakeModule.fake_function);
  enode.ex.IO.ANSI.format(["Hello", atom("red"), "World"], true);
  //enode.ex.System.stop()
  done = true;
  console.log("All tests passed. Closing node.");
  } finally {proc.kill()}
})
