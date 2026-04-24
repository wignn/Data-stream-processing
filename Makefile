


benchmark:
	k6 run -e MODE=dual -e VUS=350 -e ITERATIONS=100000 load-test/grpc_bench.js