# Performance analysis

## Lexing

Running the lexer on the generated file from the python script in "normal" mode, the following measurements were taken:

```csv
Size (MB),Tokens,Time (ms),Throughput (MB/s)
0.10,19021,2.898,34.56
0.50,94297,10.179,49.13
1.00,188187,15.624,64.02
5.00,934551,73.298,68.22
10.00,1865087,146.761,68.14
100.00,18490464,1462.060,68.40
```
