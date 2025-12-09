#!/usr/bin/env python3
"""
Simple HTTP server to test webhook delivery
Run this script to receive and log webhook events from the face recognition system

Usage:
    python3 test_webhook_receiver.py [port]

Default port: 8888

Example webhook configuration in the UI:
    URL: http://localhost:8888/webhook
    Method: POST
    Event Types: ["person_recognized", "face_detected"]
"""
import json
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime


class WebhookHandler(BaseHTTPRequestHandler):
    """HTTP request handler for receiving webhooks"""

    def do_POST(self):
        """Handle POST requests from webhook sender"""
        # Get content length
        content_length = int(self.headers.get('Content-Length', 0))

        # Read request body
        body = self.rfile.read(content_length)

        # Parse JSON payload
        try:
            payload = json.loads(body.decode('utf-8'))

            # Log received webhook
            print("\n" + "=" * 80)
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Webhook Received")
            print("=" * 80)
            print(f"Path: {self.path}")
            print(f"Headers: {dict(self.headers)}")
            print("\nPayload:")
            print(json.dumps(payload, indent=2))
            print("=" * 80 + "\n")

            # Send success response
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()

            response = {
                "success": True,
                "message": "Webhook received successfully",
                "timestamp": datetime.utcnow().isoformat()
            }
            self.wfile.write(json.dumps(response).encode('utf-8'))

        except json.JSONDecodeError as e:
            print(f"ERROR: Invalid JSON payload: {e}")

            # Send error response
            self.send_response(400)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()

            response = {
                "success": False,
                "error": "Invalid JSON payload"
            }
            self.wfile.write(json.dumps(response).encode('utf-8'))

    def log_message(self, format, *args):
        """Override to suppress default logging"""
        pass


def run_server(port=8888):
    """Start the webhook receiver server"""
    server_address = ('', port)
    httpd = HTTPServer(server_address, WebhookHandler)

    print("\n" + "=" * 80)
    print("Webhook Test Receiver Started")
    print("=" * 80)
    print(f"Listening on: http://localhost:{port}")
    print(f"Webhook URL: http://localhost:{port}/webhook")
    print("\nTo configure in the UI:")
    print(f"  1. Go to Webhooks tab")
    print(f"  2. Create new webhook with URL: http://localhost:{port}/webhook")
    print(f"  3. Set Method: POST")
    print(f"  4. Set Event Types: person_recognized, face_detected")
    print(f"  5. Save and test!")
    print("\nPress Ctrl+C to stop")
    print("=" * 80 + "\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nShutting down webhook receiver...")
        httpd.shutdown()


if __name__ == "__main__":
    # Get port from command line argument or use default
    port = 8888
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Invalid port: {sys.argv[1]}")
            print("Usage: python3 test_webhook_receiver.py [port]")
            sys.exit(1)

    run_server(port)
