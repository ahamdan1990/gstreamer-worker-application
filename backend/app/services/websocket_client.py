"""
WebSocket Client for Worker Communication
Maintains persistent connection to C++ Worker for real-time event streaming
"""
import asyncio
import json
import logging
from typing import Optional, Callable, Dict, Any
import websockets
from websockets.exceptions import ConnectionClosed, WebSocketException

logger = logging.getLogger(__name__)


class WorkerWebSocketClient:
    """
    Persistent WebSocket client for receiving events from C++ worker.
    Handles auto-reconnection and event dispatching with exponential backoff.
    """

    def __init__(
        self,
        worker_ws_url: str,
        on_event: Optional[Callable[[Dict[str, Any]], Any]] = None,
        reconnect_delay: float = 2.0,
        max_reconnect_delay: float = 60.0,
        ping_interval: float = 10.0,
        ping_timeout: float = 30.0
    ):
        """
        Initialize WebSocket client

        Args:
            worker_ws_url: WebSocket URL (e.g., "ws://localhost:8081/ws")
            on_event: Async callback function for handling events
            reconnect_delay: Initial reconnection delay in seconds
            max_reconnect_delay: Maximum reconnection delay (exponential backoff cap)
            ping_interval: Send ping every N seconds
            ping_timeout: Timeout for ping response
        """
        self.worker_ws_url = worker_ws_url
        self.on_event = on_event
        self.reconnect_delay = reconnect_delay
        self.max_reconnect_delay = max_reconnect_delay
        self.ping_interval = ping_interval
        self.ping_timeout = ping_timeout

        self.current_delay = reconnect_delay
        self.websocket: Optional[websockets.WebSocketClientProtocol] = None
        self.running = False
        self.connected = False
        self.task: Optional[asyncio.Task] = None

        # Statistics
        self.events_received = 0
        self.reconnect_count = 0
        self.last_event_time: Optional[float] = None

    async def start(self):
        """Start the WebSocket client (non-blocking)"""
        if self.running:
            logger.warning("WebSocket client already running")
            return

        self.running = True
        self.task = asyncio.create_task(self._run())
        logger.info(f"WebSocket client started, connecting to {self.worker_ws_url}")

    async def stop(self):
        """Stop the WebSocket client gracefully"""
        logger.info("Stopping WebSocket client...")
        self.running = False

        if self.websocket:
            try:
                await self.websocket.close()
            except Exception as e:
                logger.error(f"Error closing WebSocket: {e}")

        if self.task:
            self.task.cancel()
            try:
                await self.task
            except asyncio.CancelledError:
                pass

        self.connected = False
        logger.info("WebSocket client stopped")

    async def _run(self):
        """Main connection loop with auto-reconnection"""
        while self.running:
            try:
                logger.info(f"Connecting to worker WebSocket: {self.worker_ws_url}")

                async with websockets.connect(
                    self.worker_ws_url,
                    ping_interval=self.ping_interval,
                    ping_timeout=self.ping_timeout,
                    close_timeout=5.0
                ) as websocket:
                    self.websocket = websocket
                    self.connected = True
                    self.current_delay = self.reconnect_delay  # Reset delay on successful connection
                    self.reconnect_count += 1

                    logger.info(f"✅ Connected to worker WebSocket (reconnect #{self.reconnect_count})")

                    # Listen for messages
                    async for message in websocket:
                        await self._handle_message(message)

            except ConnectionClosed as e:
                logger.warning(f"WebSocket connection closed: {e.code} - {e.reason}")
                self.connected = False
                await self._reconnect()

            except WebSocketException as e:
                logger.error(f"WebSocket exception: {e}")
                self.connected = False
                await self._reconnect()

            except Exception as e:
                logger.error(f"Unexpected WebSocket error: {e}", exc_info=True)
                self.connected = False
                await self._reconnect()

    async def _reconnect(self):
        """Handle reconnection with exponential backoff"""
        if not self.running:
            return

        logger.info(f"Reconnecting in {self.current_delay:.1f} seconds... "
                   f"(reconnects: {self.reconnect_count})")

        await asyncio.sleep(self.current_delay)

        # Exponential backoff with max cap
        self.current_delay = min(
            self.current_delay * 2,
            self.max_reconnect_delay
        )

    async def _handle_message(self, message: str):
        """Parse and dispatch incoming WebSocket messages"""
        try:
            event = json.loads(message)

            self.events_received += 1
            self.last_event_time = asyncio.get_event_loop().time()

            event_type = event.get('event_type', 'unknown')
            logger.debug(f"Received event: {event_type} (total: {self.events_received})")

            # Dispatch to event handler
            if self.on_event:
                try:
                    result = self.on_event(event)
                    # Handle both sync and async callbacks
                    if asyncio.iscoroutine(result):
                        await result
                except Exception as e:
                    logger.error(f"Error in event handler for {event_type}: {e}", exc_info=True)

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON received: {e}")
            logger.debug(f"Raw message: {message[:200]}")  # Log first 200 chars
        except Exception as e:
            logger.error(f"Error handling message: {e}", exc_info=True)

    def is_connected(self) -> bool:
        """Check if WebSocket is currently connected"""
        return self.connected

    def get_stats(self) -> Dict[str, Any]:
        """Get client statistics"""
        return {
            "connected": self.connected,
            "events_received": self.events_received,
            "reconnect_count": self.reconnect_count,
            "last_event_time": self.last_event_time,
            "worker_url": self.worker_ws_url
        }
