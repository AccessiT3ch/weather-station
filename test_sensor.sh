#!/bin/bash

echo "=========================================="
echo "Testing C++ Edge Weather Station (Port 8080)"
echo "=========================================="

# Optional: uncomment if you use a build directory
# cd build && make && cd ..

echo -e "\n[1] Pinging local HTTP telemetry endpoint..."
curl -s -i http://localhost:8080/weather

echo -e "\n=========================================="
echo "Test complete."
