const erlang = require('./build/Release/erlang');
const { inherits, inspect } = require('util');

inherits(erlang.ErlangNode, require('events'));

erlang.ErlangNode.prototype.emitt = function() {
  console.log(this, arguments);
};

erlang.ErlangNode.prototype.call = function(strings, ...args) {
  let l = args.length;
  let path = "";
  for (let i = 0; i < l; i++) path += strings[i] + args[i];
  path += strings[l];
  const idx = path.lastIndexOf(':');
  if (idx === -1) return this.rpc.bind(this, "", path);
  return this.rpc.bind(this, path.substring(0, idx), path.substring(idx+1));
};

const moduleHandler = {
  get({ self, path, cache }, prop) {
    let val = cache[prop];
    if (typeof val === 'undefined') {  
      function ex(...args) { self.rpc(path, prop, ...args); }
      ex.self = self;
      ex.path = path + '.' + prop;
      ex.cache = {};
      
      cache[prop] = val = new Proxy(ex, moduleHandler);
    }
    return val;
  }
};

Object.defineProperty(erlang.ErlangNode.prototype, "ex", {
  get: () => new Proxy({ self: this, path: "Elixir", cache: {} }, {
    get({ self, path, cache }, prop) {
      let val = cache[prop];
      if (typeof val === 'undefined') {
        cache[prop] = val = new Proxy(
          { self, path: path + '.' + prop, cache: {} }, moduleHandler,
        );
      }
      return val;
    },
  })
});

erlang.charlist = (str) => Array.from(str, char => {
  const code = char.charCodeAt(0);
  if(code < 0 || code > 255) throw new Error("Invalid character: " + char);
  return code;
});

erlang.tuple = (...args) => new erlang.Tuple(args);
erlang.atom = (a) => new erlang.Atom(a);

erlang.Tuple.prototype[inspect.custom] = function (_depth, _options) {
  return "Tuple {" + inspect(this.value).slice(1,-1)  + "}";
};

erlang.Atom.prototype[inspect.custom] = function(_depth, _options) {
  return "Atom { " + inspect(this.value) + " }";
};

module.exports = erlang;