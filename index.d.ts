import Events from 'events';

type ElixirFunction = {
  [key: string]: ElixirFunction;
} & ((...args: any[]) => void);

export declare class ErlangNode extends Events.EventEmitter {
  emitt(...args: any[]): void;
  rpc(module: string, fname: string, ...args: any): void;
  disconnect(): void;
  node_name: string;
  ex: {
    [key: string]: ElixirFunction; 
  };
}

export declare class Tuple<U extends any[]> {
  constructor(value: U);
  value: U;
  length: number;
}

export declare class Atom {
  constructor(value: string);
  value: string;
  toString(): string;
}

export declare function charlist(str: string): number[];
export declare function tuple<U extends any[]>(...args: U): Tuple<U>;
export declare function atom(a: string): Atom;