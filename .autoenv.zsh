alias pluma="docker -v $(pwd)/plugins:/app/plugins -v $(pwd)/pipelines:/app/pipelines -v $(pwd)/config:/app/config -v $(pwd)/logs:/app/logs -t pluma:latest"
