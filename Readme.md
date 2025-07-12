## To build

```bash
g++ -std=c++17 -O3 ip_calculator.cpp calculations.cpp -o build/ip_calculator 
```

## To run

```bash
./ip_calculator -i 0.0.0.0/0 -e 192.168.0.0/16 --prefix-length --delimiter ', ' -v
```

vibe coded with vscode and github copilot (gemini 2.5 pro)

The key to the algorithm is treating the ip range as actual "range" with start and end.
e.g.) 192.168.0.0/8 --> [192.168.0.0 - 192.168.0.255]
after that, merging and subtracting is just manipulating the boundaries.