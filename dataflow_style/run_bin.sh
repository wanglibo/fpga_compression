#!/bin/bash

cp host_bin_hw_backup/pkg/pcie/deflate1.xclbin host_only/pkg/pcie &&
cd host_only/pkg/pcie && ./host_only.exe
