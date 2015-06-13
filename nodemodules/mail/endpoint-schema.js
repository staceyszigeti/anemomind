var schemautils = require('./schemautils.js');

var MethodSchema = schemautils.MethodSchema;
var EndPointSchema = schemautils.EndPointSchema;
var errorTypes = schemautils.errorTypes;
  
var methods = {};

methods.putPacket = new MethodSchema({
  httpMethod: 'post',
  input: [{packet: 'any'}],
  output: [{err: errorTypes}]
});

// Only for debugging and unit testing
methods.sendPacket = new MethodSchema({
    httpMethod: 'post',
    input: [
	{dst: String},
	{label: Number},
	{data: 'buffer'}
    ],
    output: [
	{err: errorTypes}
    ]
});

methods.getSrcDstPairs = new MethodSchema({
  httpMethod:'get',
  input: [],
  output: [
    {err: errorTypes}
  ]
});

methods.setLowerBound = new MethodSchema({
  httpMethod: 'get',
  input: [
    {src: String},
    {dst: String},
    {lowerBound: 'hex'}
  ],
  output: [{err: errorTypes}]
});

methods.getLowerBound = new MethodSchema({
  httpMethod: 'get',
  input: [
    {src: String},
    {dst: String},
  ],
  output: [
    {err: errorTypes},
    {lowerBound: 'hex'}
  ]
});

methods.getUpperBound = new MethodSchema({
  httpMethod: 'get',
  input: [
    {src: String},
    {dst: String},
  ],
  output: [
    {err: errorTypes},
    {lowerBound: 'hex'}
  ]
});


methods.getTotalPacketCount = new MethodSchema({
    httpMethod: 'get',
    input: [],
    output: [
	{err: errorTypes},
	{count: Number}
    ]
});

methods.reset = new MethodSchema({
    httpMethod:'get',
    input: [],
    output: [
	{err: errorTypes}
    ]
});
