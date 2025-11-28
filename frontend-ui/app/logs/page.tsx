'use client';

import { useState, useEffect, useRef } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Layout, PageHeader } from '@/components/layout';
import { Trash2, Pause, Play, Download, Settings, ChevronDown, ChevronUp } from 'lucide-react';
import { Label } from '@/components/ui/label';
import { Alert, AlertDescription } from '@/components/ui/alert';

interface LogEntry {
  timestamp: number;
  event_type: string;
  level?: string;
  component?: string;
  message?: string;
  camera_id?: string;
  data?: any;
}

// Event categories for filtering
const EVENT_CATEGORIES = {
  motion: ['motion_detected'],
  face: ['face_detected', 'person_recognized'],
  camera: ['camera_started', 'camera_stopped', 'camera_error'],
  system: ['worker_started', 'worker_stopped', 'pipeline_crashed', 'fps_drop'],
  logs: ['log', 'INFO', 'DEBUG', 'WARNING', 'ERROR']
};

export default function LogsPage() {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [isPaused, setIsPaused] = useState(false);
  const [autoScroll, setAutoScroll] = useState(true);
  const [filterLevel, setFilterLevel] = useState<string>('all');
  const [filterCategory, setFilterCategory] = useState<string>('all');
  const [showSettings, setShowSettings] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [excludedTypes, setExcludedTypes] = useState<Set<string>>(new Set());
  const logsEndRef = useRef<HTMLDivElement>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const isInitialMount = useRef(true);

  // Load excluded types from localStorage on mount
  useEffect(() => {
    const saved = localStorage.getItem('excludedLogTypes');
    if (saved) {
      setExcludedTypes(new Set(JSON.parse(saved)));
    }
  }, []);

  // Save excluded types to localStorage when changed (skip on initial mount)
  useEffect(() => {
    if (isInitialMount.current) {
      isInitialMount.current = false; // Mark as no longer initial mount
      return; // Don't save on initial mount
    }
    localStorage.setItem('excludedLogTypes', JSON.stringify([...excludedTypes]));
  }, [excludedTypes]);

  useEffect(() => {
    const connectWebSocket = () => {
      const ws = new WebSocket('ws://192.168.0.24:8082/ws');
      wsRef.current = ws;

      ws.onopen = () => {
        console.log('Connected to worker WebSocket');
        setWsConnected(true);
      };

      ws.onmessage = (event) => {
        if (isPaused) return;
        try {
          const data = JSON.parse(event.data);
          // console.log('Received event:', data);
          if (!data.timestamp) {
            data.timestamp = Date.now() / 1000;
          }

          // Check if this event type is excluded
          const eventType = data.level || data.event_type;
          if (excludedTypes.has(eventType)) {
            //console.log('Filtered out excluded event:', eventType);
            return;
          }

          setLogs((prev) => [...prev.slice(-500), data]);
        } catch (error) {
          console.error('Failed to parse log message:', error);
        }
      };

      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        setWsConnected(false);
      };

      ws.onclose = () => {
        console.log('WebSocket disconnected');
        setWsConnected(false);
        setTimeout(() => {
          if (!wsRef.current || wsRef.current.readyState === WebSocket.CLOSED) {
            connectWebSocket();
          }
        }, 2000);
      };
    };

    connectWebSocket();
    return () => {
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [isPaused, excludedTypes]);

  useEffect(() => {
    if (autoScroll && logsEndRef.current) {
      logsEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, autoScroll]);

  const clearLogs = () => setLogs([]);
  
  const downloadLogs = () => {
    const logText = logs.map((log) =>
      `[${new Date(log.timestamp * 1000).toISOString()}] ${log.level || log.event_type} ${
        log.component ? `[${log.component}]` : ''
      } ${log.camera_id ? `[${log.camera_id}]` : ''} ${log.message || JSON.stringify(log.data)}`
    ).join('\n');
    const blob = new Blob([logText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `worker-logs-${new Date().toISOString()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const getLogColor = (log: LogEntry) => {
    const level = log.level || log.event_type;
    if (level === 'ERROR' || level === 'pipeline_crashed') return 'text-red-600 bg-red-50';
    if (level === 'WARNING' || level === 'fps_drop') return 'text-yellow-600 bg-yellow-50';
    if (level === 'INFO' || level === 'face_detected' || level === 'person_recognized') return 'text-blue-600 bg-blue-50';
    if (level === 'DEBUG' || level === 'motion_detected') return 'text-gray-600 bg-gray-50';
    return 'text-gray-800 bg-white';
  };

  const getLevelBadge = (log: LogEntry) => {
    const level = log.level || log.event_type;
    if (level === 'ERROR' || level === 'pipeline_crashed') return 'destructive';
    if (level === 'WARNING' || level === 'fps_drop') return 'secondary';
    if (level === 'INFO' || level === 'face_detected' || level === 'person_recognized') return 'default';
    return 'outline';
  };

  const filteredLogs = logs.filter((log) => {
    const eventType = log.level || log.event_type;

    // Filter by level
    if (filterLevel !== 'all' && eventType !== filterLevel) {
      return false;
    }

    // Filter by category
    if (filterCategory !== 'all') {
      const categoryEvents = EVENT_CATEGORIES[filterCategory as keyof typeof EVENT_CATEGORIES] || [];
      if (!categoryEvents.includes(eventType)) {
        return false;
      }
    }

    return true;
  });

  return (
    <Layout wsConnected={wsConnected}>
      <PageHeader
        title="Worker Logs"
        description="Real-time logs and events from C++ GStreamer Worker"
        action={
          <div className="flex items-center gap-2">
            <Badge variant={wsConnected ? 'default' : 'secondary'} className={wsConnected ? 'bg-green-500' : 'bg-gray-400'}>
              {wsConnected ? '● Live' : '○ Disconnected'}
            </Badge>
            <span className="text-sm text-gray-500">{filteredLogs.length} events</span>
          </div>
        }
      />
      <div className="p-8">
        {/* Filter Settings Panel */}
        <Card className="mb-6">
          <CardHeader className="pb-3">
            <div className="flex items-center justify-between">
              <CardTitle className="text-base">Filters & Settings</CardTitle>
              <Button variant="ghost" size="sm" onClick={() => setShowSettings(!showSettings)}>
                {showSettings ? <ChevronUp className="h-4 w-4" /> : <ChevronDown className="h-4 w-4" />}
              </Button>
            </div>
          </CardHeader>
          <CardContent className="space-y-4">
            {/* Level Filter */}
            <div>
              <Label className="text-sm font-semibold mb-2 block">Filter by Level</Label>
              <div className="flex flex-wrap gap-2">
                <Button variant={filterLevel === 'all' ? 'default' : 'outline'} onClick={() => setFilterLevel('all')} size="sm">
                  All ({logs.length})
                </Button>
                <Button variant={filterLevel === 'ERROR' ? 'default' : 'outline'} onClick={() => setFilterLevel('ERROR')} size="sm">
                  Errors
                </Button>
                <Button variant={filterLevel === 'WARNING' ? 'default' : 'outline'} onClick={() => setFilterLevel('WARNING')} size="sm">
                  Warnings
                </Button>
                <Button variant={filterLevel === 'INFO' ? 'default' : 'outline'} onClick={() => setFilterLevel('INFO')} size="sm">
                  Info
                </Button>
                <Button variant={filterLevel === 'DEBUG' ? 'default' : 'outline'} onClick={() => setFilterLevel('DEBUG')} size="sm">
                  Debug
                </Button>
              </div>
            </div>

            {/* Category Filter */}
            <div>
              <Label className="text-sm font-semibold mb-2 block">Filter by Category</Label>
              <div className="flex flex-wrap gap-2">
                <Button variant={filterCategory === 'all' ? 'default' : 'outline'} onClick={() => setFilterCategory('all')} size="sm">
                  All Events
                </Button>
                <Button variant={filterCategory === 'motion' ? 'default' : 'outline'} onClick={() => setFilterCategory('motion')} size="sm">
                  Motion
                </Button>
                <Button variant={filterCategory === 'face' ? 'default' : 'outline'} onClick={() => setFilterCategory('face')} size="sm">
                  Face Detection
                </Button>
                <Button variant={filterCategory === 'camera' ? 'default' : 'outline'} onClick={() => setFilterCategory('camera')} size="sm">
                  Camera Events
                </Button>
                <Button variant={filterCategory === 'system' ? 'default' : 'outline'} onClick={() => setFilterCategory('system')} size="sm">
                  System
                </Button>
                <Button variant={filterCategory === 'logs' ? 'default' : 'outline'} onClick={() => setFilterCategory('logs')} size="sm">
                  General Logs
                </Button>
              </div>
            </div>

            {/* Event Exclusion Settings Link */}
            {showSettings && (
              <div className="pt-4 border-t">
                <Alert>
                  <Settings className="h-4 w-4" />
                  <AlertDescription>
                    <strong>Configure Event Exclusions:</strong><br />
                    <span className="text-sm">
                      To exclude specific event types from being logged, go to{' '}
                      <a href="/settings" className="text-blue-600 hover:underline font-semibold">
                        Settings → Logs Settings
                      </a>
                      . Currently {excludedTypes.size} event type{excludedTypes.size !== 1 ? 's' : ''} excluded.
                    </span>
                  </AlertDescription>
                </Alert>
              </div>
            )}
          </CardContent>
        </Card>

        {/* Controls */}
        <div className="mb-6 flex flex-wrap items-center gap-2">
          <div className="ml-auto flex gap-2">
            <Button variant="outline" onClick={() => setAutoScroll(!autoScroll)} size="sm">
              {autoScroll ? '🔽 Auto-scroll ON' : '⏸️ Auto-scroll OFF'}
            </Button>
            <Button variant="outline" onClick={() => setIsPaused(!isPaused)} size="sm">
              {isPaused ? <Play className="h-4 w-4 mr-2" /> : <Pause className="h-4 w-4 mr-2" />}
              {isPaused ? 'Resume' : 'Pause'}
            </Button>
            <Button variant="outline" onClick={downloadLogs} size="sm">
              <Download className="h-4 w-4 mr-2" />Download
            </Button>
            <Button variant="outline" onClick={clearLogs} size="sm">
              <Trash2 className="h-4 w-4 mr-2" />Clear
            </Button>
          </div>
        </div>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center justify-between">
              <span>Real-time Logs</span>
              <span className="text-sm font-normal text-gray-500">WebSocket: ws://192.168.0.24:8082/ws</span>
            </CardTitle>
          </CardHeader>
          <CardContent className="h-[calc(100vh-350px)] overflow-y-auto">
            {filteredLogs.length === 0 ? (
              <div className="flex flex-col items-center justify-center h-full text-gray-500">
                <p className="text-lg font-semibold">No logs yet</p>
                <p className="text-sm mt-2">{wsConnected ? 'Waiting for events from worker...' : 'Connecting to worker WebSocket...'}</p>
              </div>
            ) : (
              <div className="font-mono text-xs space-y-1">
                {filteredLogs.map((log, index) => {
                  const rawImageUrl = log.data?.image_url;
                  // Clean up image URL: remove ./ prefix and ensure it starts with /
                  const imageUrl = rawImageUrl ? rawImageUrl.replace(/^\.\//, '/').replace(/^(?!\/)/, '/') : null;
                  const isFaceEvent = log.event_type === 'face_detected' || log.event_type === 'person_recognized';

                  return (
                    <div key={index} className={`p-2 rounded ${getLogColor(log)}`}>
                      <div className="flex items-start gap-2 flex-wrap">
                        {/* Face Image Thumbnail */}
                        {isFaceEvent && imageUrl && (
                          <div className="w-12 h-12 rounded border border-gray-300 overflow-hidden bg-gray-100 flex-shrink-0">
                            <img
                              src={`http://192.168.0.24:8002${imageUrl}`}
                              alt="Face"
                              className="w-full h-full object-cover"
                              onError={(e) => {
                                const target = e.currentTarget as HTMLImageElement;
                                target.style.display = 'none';
                              }}
                            />
                          </div>
                        )}

                        <span className="text-gray-500 min-w-[180px]">
                          {new Date(log.timestamp * 1000).toLocaleTimeString()}.{String(Math.floor((log.timestamp % 1) * 1000)).padStart(3, '0')}
                        </span>
                        <Badge variant={getLevelBadge(log)} className="min-w-[80px] justify-center">{log.level || log.event_type}</Badge>
                        {log.component && <Badge variant="outline" className="min-w-[120px] justify-center">{log.component}</Badge>}
                        {log.camera_id && <Badge variant="outline" className="min-w-[100px] justify-center">{log.camera_id}</Badge>}
                        <span className="flex-1 break-all">{log.message || JSON.stringify(log.data, null, 2).substring(0, 200)}</span>
                      </div>
                    </div>
                  );
                })}
                <div ref={logsEndRef} />
              </div>
            )}
          </CardContent>
        </Card>
      </div>
    </Layout>
  );
}
