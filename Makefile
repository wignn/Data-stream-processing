


benchmark:
	k6 run -e MODE=dual -e VUS=50 -e ITERATIONS=10000 load-test/grpc_bench.js