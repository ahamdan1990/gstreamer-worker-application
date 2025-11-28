'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Camera, Activity, AlertCircle, Play, Pause, RefreshCw, Plus, TrendingUp } from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';
import apiClient from '@/lib/api';
import wsClient from '@/lib/websocket';
import type { Camera as CameraType, SystemStatus, WebSocketEvent } from '@/lib/types';

export default function DashboardPage() {
  const [cameras, setCameras] = useState<CameraType[]>([]);
  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>(null);
  const [events, setEvents] = useState<WebSocketEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [wsConnected, setWsConnected] = useState(false);

  useEffect(() => {
    loadData();
    const interval = setInterval(() => setWsConnected(wsClient.isConnected()), 1000);

    wsClient.connect();
    const unsubscribe = wsClient.subscribe((event) => {
      //console.log('WebSocket event:', event);
      setEvents(prev => [event, ...prev].slice(0, 10));

      // Handle camera state updates
      if (event.camera_id && event.event_type === 'camera_started') {
        setCameras(prev => prev.map(cam =>
          cam.camera_id === event.camera_id ? {...cam, is_running: true, state: 'RUNNING'} : cam
        ));
      } else if (event.camera_id && event.event_type === 'camera_stopped') {
        setCameras(prev => prev.map(cam =>
          cam.camera_id === event.camera_id ? {...cam, is_running: false, state: 'STOPPED'} : cam
        ));
      } else if (event.event_type === 'camera_state_update' && event.camera_id) {
        // Handle real-time state updates from worker
        setCameras(prev => prev.map(cam =>
          cam.camera_id === event.camera_id
            ? { ...cam, state: event.data.state || cam.state, is_running: event.data.is_running ?? cam.is_running }
            : cam
        ));
      }
    });

    return () => { unsubscribe(); wsClient.disconnect(); clearInterval(interval); };
  }, []);

  const loadData = async () => {
    try {
      setLoading(true);
      const [camerasData, statusData] = await Promise.all([apiClient.getCameras(), apiClient.getSystemStatus()]);
      setCameras(camerasData);
      setSystemStatus(statusData);
    } catch (error) {
      console.error('Failed to load data:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleToggleCamera = async (camera: CameraType) => {
    try {
      if (camera.is_running) {
        await apiClient.stopCamera(camera.camera_id);
      } else {
        await apiClient.startCamera(camera.camera_id);
      }
      await loadData();
    } catch (error) {
      console.error('Failed to toggle camera:', error);
      alert(`Failed to ${camera.is_running ? 'stop' : 'start'} camera: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  if (loading) {
    return (
      <Layout wsConnected={wsConnected}>
        <div className="flex items-center justify-center min-h-screen">
          <RefreshCw className="h-8 w-8 animate-spin text-primary" />
        </div>
      </Layout>
    );
  }

  const runningCameras = cameras.filter(c => c.is_running).length;
  const errorCameras = cameras.filter(c => c.state === 'ERROR').length;

  return (
    <Layout wsConnected={wsConnected}>
      <PageHeader
        title="Dashboard"
        description="Real-time monitoring and control"
        action={
          <Button onClick={loadData} variant="outline" size="sm">
            <RefreshCw className="h-4 w-4 mr-2" />
            Refresh
          </Button>
        }
      />

      <main className="container mx-auto px-6 py-8">
        {/* Action Bar */}
        <div className="flex items-center justify-between mb-6">
          <div>
            <h2 className="text-lg font-semibold text-slate-900 dark:text-white">Overview</h2>
            <p className="text-sm text-slate-600 dark:text-slate-400">Monitor your camera system</p>
          </div>
          <Link href="/cameras/add">
            <Button>
              <Plus className="h-4 w-4 mr-2" />
              Add Camera
            </Button>
          </Link>
        </div>

        {/* Stats Cards */}
        <div className="grid gap-6 md:grid-cols-4 mb-8">
          <Card className="border-l-4 border-l-blue-500">
            <CardHeader className="flex flex-row items-center justify-between pb-2">
              <CardTitle className="text-sm font-medium">Total Cameras</CardTitle>
              <Camera className="h-5 w-5 text-blue-500" />
            </CardHeader>
            <CardContent>
              <div className="text-3xl font-bold">{cameras.length}</div>
              <p className="text-xs text-green-600 mt-1">● {runningCameras} active</p>
            </CardContent>
          </Card>

          <Card className="border-l-4 border-l-green-500">
            <CardHeader className="flex flex-row items-center justify-between pb-2">
              <CardTitle className="text-sm font-medium">System Health</CardTitle>
              <Activity className="h-5 w-5 text-green-500" />
            </CardHeader>
            <CardContent>
              <div className="text-3xl font-bold text-green-600">{systemStatus?.running ? 'Healthy' : 'Offline'}</div>
              <p className="text-xs text-muted-foreground mt-1">All systems operational</p>
            </CardContent>
          </Card>

          <Card className="border-l-4 border-l-orange-500">
            <CardHeader className="flex flex-row items-center justify-between pb-2">
              <CardTitle className="text-sm font-medium">Live Events</CardTitle>
              <AlertCircle className="h-5 w-5 text-orange-500" />
            </CardHeader>
            <CardContent>
              <div className="text-3xl font-bold">{events.length}</div>
              <p className="text-xs text-muted-foreground mt-1">Recent activity</p>
            </CardContent>
          </Card>

          <Card className="border-l-4 border-l-red-500">
            <CardHeader className="flex flex-row items-center justify-between pb-2">
              <CardTitle className="text-sm font-medium">Errors</CardTitle>
              <AlertCircle className="h-5 w-5 text-red-500" />
            </CardHeader>
            <CardContent>
              <div className="text-3xl font-bold text-red-600">{errorCameras}</div>
              <p className="text-xs text-muted-foreground mt-1">Cameras with errors</p>
            </CardContent>
          </Card>
        </div>

        {/* Camera Cards */}
        <div className="grid gap-6 md:grid-cols-2 lg:grid-cols-3">
          {cameras.map(camera => {
            const stateColor = camera.state === 'RUNNING' ? 'bg-green-500' :
                              camera.state === 'ERROR' ? 'bg-red-500' :
                              camera.state === 'STARTING' ? 'bg-yellow-500' :
                              'bg-slate-500';

            return (
              <Card key={camera.id} className={`hover:shadow-xl transition-all duration-300 border-2 ${camera.state === 'ERROR' ? 'border-red-300' : 'hover:border-blue-300'}`}>
                <CardHeader>
                  <div className="flex items-start justify-between">
                    <div className="flex-1">
                      <CardTitle className="text-lg flex items-center gap-2">
                        <Camera className="h-5 w-5" />
                        {camera.name}
                      </CardTitle>
                      <CardDescription className="mt-1 text-xs">{camera.camera_id}</CardDescription>
                    </div>
                    <Badge variant={camera.is_running ? "default" : "secondary"} className={stateColor}>
                      {camera.state}
                    </Badge>
                  </div>
                </CardHeader>
                <CardContent className="space-y-4">
                  <div className="grid grid-cols-2 gap-3 text-sm">
                    <div><div className="text-muted-foreground text-xs">Location</div><div className="font-medium truncate">{camera.location || 'N/A'}</div></div>
                    <div><div className="text-muted-foreground text-xs">Target FPS</div><div className="font-medium">{camera.target_fps}</div></div>
                  </div>

                  {camera.state === 'ERROR' && (
                    <div className="rounded-md bg-red-50 dark:bg-red-900/20 p-3 text-xs text-red-600 dark:text-red-400">
                      Camera failed to sync. Check RTSP URL and credentials.
                    </div>
                  )}

                  <div className="flex gap-2">
                    <Button
                      onClick={() => handleToggleCamera(camera)}
                      variant={camera.is_running ? "destructive" : "default"}
                      size="sm"
                      className="flex-1"
                      disabled={camera.state === 'ERROR'}
                    >
                      {camera.is_running ? <><Pause className="h-4 w-4 mr-2" />Stop</> : <><Play className="h-4 w-4 mr-2" />Start</>}
                    </Button>
                    <Link href={`/cameras/${camera.camera_id}`}>
                      <Button variant="outline" size="sm">
                        View
                      </Button>
                    </Link>
                  </div>
                </CardContent>
              </Card>
            );
          })}
        </div>

        {cameras.length === 0 && (
          <Card className="mt-12">
            <CardContent className="flex flex-col items-center justify-center py-16">
              <Camera className="h-16 w-16 text-muted-foreground mb-4" />
              <h3 className="text-xl font-semibold mb-2">No Cameras Found</h3>
              <p className="text-sm text-muted-foreground mb-4">Get started by adding your first camera</p>
              <Link href="/cameras/add">
                <Button>
                  <Plus className="h-4 w-4 mr-2" />
                  Add Camera
                </Button>
              </Link>
            </CardContent>
          </Card>
        )}
      </main>
    </Layout>
  );
}
