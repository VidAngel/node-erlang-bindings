module.exports = require('./build/Release/erlang');
require('util').inherits(module.exports.ErlangNode, require('events'));
module.exports.ErlangNode.prototype.emitt = function() {
  console.log(this, arguments);
};
module.exports.charlist = (str) => {
  const list = [];
  // "A string [charlist] in Erlang is a list of integers between 0 and 255"
  for(let i = 0; i<str.length; i++) {
    const code = str.charCodeAt(i)
    if(code < 0 || code > 255) throw new Error("Invalid character: " + str[i]);
    list.push(code);
  }
  return list;
};
const moduleHandler = {
  get: function(obj, prop) {
    if(prop[0].toUpperCase() == prop[0]) {
      return new Proxy(Object.assign(obj, {path: obj.path + "." + prop}), moduleHandler);
    }
    return function() {
      return obj.self.rpc(obj.path, prop, ...arguments);
    }
  }
};

Object.defineProperty(module.exports.ErlangNode.prototype, "ex", {
  get: function() {
    return new Proxy({path: "Elixir", self: this}, moduleHandler);
  }
});
module.exports.tuple = function tupleFactory(...args) { return new module.exports.Tuple(args); }
module.exports.atom = function atomFactory(a) {return new module.exports.Atom(a); }

const util = require('util');
module.exports.Tuple.prototype[util.inspect.custom] = function(depth, options){
  return "Tuple {" + util.inspect(this.value).slice(1,-1)  + "}"
}
module.exports.Atom.prototype[util.inspect.custom] = function(depth, options){
  return "Atom { " + util.inspect(this.value) + " }"
}
