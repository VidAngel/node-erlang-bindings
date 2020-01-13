# Erlang Bindings for Node.js

Call functions on a remote Erlang node from JavaScript.

Requires Erlang development headers, usually included in a typical Erlang
client installation.

## Usage

See [test-napi.js](test-napi.js) for a more complete list of examples

If you spawn a node in Erlang called remotenode@127.0.0.1 with cookie "mycookie",
you can connect to it in Node.js with the following code

```javascript
const {ErlangNode,tuple,charlist} = require('erlang-bindings');
const mynode = new ErlangNode("mynodename@127.0.0.1", "mycookie", "remotenode@127.0.0.1");
mynode.connect();
// do something
mynode.disconnect();
```

You can run remote calls using member function `rpc(module, function name, arg1, ...)`

```javascript
console.log(mynode.rpc('erlang', 'max', 3, 8)) // 8
```

### Tuples

Tuples can be sent by using the `tuple()` function

```javascript
const {tuple} = require('erlang-bindings');
console.log(mynode.rpc('erlang', 'tuple_size', tuple(5,2,"hello"))) // 3
```

Currently any tuples that are returned by the RPC are automatically converted
into arrays.

### Atoms

Atoms are represented in Node.js as Symbols. To send an atom as an argument,
just wrap it in a `Symbol()` function.

Note that in Javascript, `Symbol('a') != Symbol('a')`. In the future we plan
on making JS atoms an extended class of strings, analogous to how tuples are
represented in JS.

### Charlists

To represent a charlist in JavaScript, use `charlist(str)` from the package,
which will convert your string into an array of character points. Only works
for ASCII.

```javascript
const {charlist} = require('erlang-bindings');
console.log(charlist("ABC")); // [65,66,67]
```

### Maps

JavaScript objects are automatically converted into erlang maps

JavaScript: `{a: 3}`
Erlang: `%{'a' => 3}`

If you need atoms for keys, use Symbols:

JavaScript: `const map = {}; map[Symbol('a')] = 3`
Erlang: `%{a: 3}`

See the section on atoms for caveats.

### Elixir

Remote connections to Elixir nodes offer conva special provisioning allowing
RPC calls to modules under the Elixir module to be written as if they were
native Node.js function calls.

```javascript
node.ex.String.split("1864-05-23","-")
// is identical to
node.rpc("Elixir.String", "split", "1864-05-23", "-")
```
