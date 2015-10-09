#!/usr/bin/python
import sys

# Given a binary huffman tree output, perform hexdump to 64-byte lines.

def main():
	if len(sys.argv) < 2:
		print 'usage: hex_tree [tree_bin]'
		exit(1)

	tree_file = open(sys.argv[1], 'r')
	tree_bytes = tree_file.read()
	tree_bytes_512 = [None] * 512
	for i in range(512):
		if i < len(tree_bytes):
			tree_bytes_512[i] = format(ord(tree_bytes[i]), '02x')
		else:
			tree_bytes_512[i] = format(0, '02x')

	width = 64
	batch = 512 / width
	for i in range(batch):
		line = ''
		for j in range(width):
			line = line + tree_bytes_512[i*width+width-1-j]
			#line = line + tree_bytes_512[i*(width)+j] + ', '
		line = 'uint512("' + line + '", 16),'
		print line

if __name__=='__main__':
	main()
