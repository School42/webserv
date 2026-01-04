#!/bin/bash
# Webserv Test Environment Setup Script
# This script sets up the test files in /tmp/www

set -e

echo "🚀 Setting up Webserv Test Environment..."
echo ""

# Create directories
echo "📁 Creating directories..."
mkdir -p /tmp/www/html/errors
mkdir -p /tmp/www/html/images
mkdir -p /tmp/www/cgi-bin
mkdir -p /tmp/www/uploads
mkdir -p /tmp/www/downloads

# Set permissions
chmod 755 /tmp/www/html
chmod 755 /tmp/www/cgi-bin
chmod 777 /tmp/www/uploads
chmod 755 /tmp/www/downloads

# Copy files (assuming this script is in the www directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "📄 Copying HTML files..."
cp -r "$SCRIPT_DIR/html/"* /tmp/www/html/ 2>/dev/null || true

echo "⚙️  Copying CGI scripts..."
cp -r "$SCRIPT_DIR/cgi-bin/"* /tmp/www/cgi-bin/ 2>/dev/null || true

echo "📝 Copying configuration..."
cp "$SCRIPT_DIR/test.conf" /tmp/www/ 2>/dev/null || true

# Make CGI scripts executable
echo "🔧 Setting CGI permissions..."
chmod +x /tmp/www/cgi-bin/*.py 2>/dev/null || true
chmod +x /tmp/www/cgi-bin/*.php 2>/dev/null || true
chmod +x /tmp/www/cgi-bin/*.sh 2>/dev/null || true
chmod +x /tmp/www/cgi-bin/*.cgi 2>/dev/null || true
chmod +x /tmp/www/cgi-bin/*.pl 2>/dev/null || true

# Create some test files in images directory
echo "🖼️  Creating sample files..."
echo "Sample image placeholder" > /tmp/www/html/images/sample.txt
echo "Another test file" > /tmp/www/downloads/readme.txt

echo ""
echo "✅ Setup complete!"
echo ""
echo "📂 Test environment created at: /tmp/www/"
echo ""
echo "To run the server:"
echo "  ./webserv /tmp/www/test.conf"
echo ""
echo "Then open in browser:"
echo "  http://localhost:8080/"
echo ""
echo "Test endpoints:"
echo "  - Static files:     http://localhost:8080/"
echo "  - Directory list:   http://localhost:8080/images/"
echo "  - Python CGI:       http://localhost:8080/cgi-bin/hello.py"
echo "  - PHP CGI:          http://localhost:8080/cgi-bin/hello.php"
echo "  - Shell CGI:        http://localhost:8080/cgi-bin/hello.sh"
echo "  - File upload:      http://localhost:8080/upload"
echo "  - View uploads:     http://localhost:8080/uploads/"
echo "  - Redirect test:    http://localhost:8080/old-page"
echo "  - 404 test:         http://localhost:8080/nonexistent"
echo ""
