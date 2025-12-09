'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import {
  Camera,
  Plus,
  Edit,
  Trash2,
  Play,
  Pause,
  RefreshCw,
  Eye
} from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';
import apiClient from '@/lib/api';
import wsClient from '@/lib/websocket';
import type { Camera as CameraType } from '@/lib/types';

export default function CamerasPage() {
  const [cameras, setCameras] = useState<CameraType[]>([]);
  const [loading, setLoading] = useState(true);
  const [wsConnected, setWsConnected] = useState(false);

  useEffect(() => {
    loadCameras();
    const interval = setInterval(() => setWsConnected(wsClient.isConnected()), 1000);

    wsClient.connect();
    const unsubscribe = wsClient.subscribe((event) => {
      // Update camera states in real-time
      if (event.camera_id && (event.event_type === 'camera_started' || event.event_type === 'camera_stopped' || event.event_type === 'camera_state_update')) {
        setCameras(prev => prev.map(cam =>
          cam.camera_id === event.camera_id
            ? {
                ...cam,
                state: event.data.state || (event.event_type === 'camera_started' ? 'RUNNING' : 'STOPPED'),
                is_running: event.data.is_running ?? (event.event_type === 'camera_started')
              }
            : cam
        ));
      }
    });

    return () => { unsubscribe(); clearInterval(interval); };
  }, []);

  const loadCameras = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getCameras();
      setCameras(data);
    } catch (error) {
      console.error('Failed to load cameras:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (cameraId: string) => {
    if (!confirm('Are you sure you want to delete this camera?')) {
      return;
    }

    try {
      await apiClient.deleteCamera(cameraId);
      await loadCameras();
    } catch (error) {
      console.error('Failed to delete camera:', error);
      alert('Failed to delete camera');
    }
  };

  const handleToggle = async (camera: CameraType) => {
    try {
      if (camera.is_running) {
        await apiClient.stopCamera(camera.camera_id);
      } else {
        await apiClient.startCamera(camera.camera_id);
      }
      await loadCameras();
    } catch (error) {
      console.error('Failed to toggle camera:', error);
      alert(`Failed to ${camera.is_running ? 'stop' : 'start'} camera`);
    }
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  return (
    <Layout wsConnected={wsConnected}>
      <PageHeader
        title="Cameras"
        description="Manage your camera fleet"
        action={
          <Button onClick={loadCameras} variant="outline" size="sm">
            <RefreshCw className="h-4 w-4 mr-2" />
            Refresh
          </Button>
        }
      />

      <main className="container mx-auto px-6 py-8">
        <div className="flex items-center justify-between mb-6">
          <div>
            <h2 className="text-lg font-semibold text-slate-900 dark:text-white">
              All Cameras ({cameras.length})
            </h2>
            <p className="text-sm text-slate-600 dark:text-slate-400">
              View and manage all cameras
            </p>
          </div>
          <Link href="/cameras/add">
            <Button>
              <Plus className="h-4 w-4 mr-2" />
              Add Camera
            </Button>
          </Link>
        </div>

        {cameras.length === 0 ? (
          <Card>
            <CardContent className="flex flex-col items-center justify-center py-16">
              <Camera className="h-16 w-16 text-muted-foreground mb-4" />
              <h3 className="text-xl font-semibold mb-2">No Cameras Yet</h3>
              <p className="text-sm text-muted-foreground mb-4">
                Add your first camera to get started
              </p>
              <Link href="/cameras/add">
                <Button>
                  <Plus className="h-4 w-4 mr-2" />
                  Add Camera
                </Button>
              </Link>
            </CardContent>
          </Card>
        ) : (
          <div className="grid gap-4">
            {cameras.map(camera => {
              const stateColor = camera.state === 'RUNNING' ? 'bg-green-500' :
                                camera.state === 'ERROR' ? 'bg-red-500' :
                                camera.state === 'STARTING' ? 'bg-yellow-500' :
                                'bg-slate-500';

              return (
                <Card key={camera.id} className="hover:shadow-lg transition-shadow">
                  <CardContent className="p-6">
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-4 flex-1">
                        <div className="h-12 w-12 rounded-lg bg-blue-100 dark:bg-blue-900/20 flex items-center justify-center">
                          <Camera className="h-6 w-6 text-blue-600" />
                        </div>
                        <div className="flex-1">
                          <div className="flex items-center gap-2">
                            <h3 className="text-lg font-semibold text-slate-900 dark:text-white">
                              {camera.name}
                            </h3>
                            <Badge className={stateColor}>
                              {camera.state}
                            </Badge>
                          </div>
                          <p className="text-sm text-slate-600 dark:text-slate-400">
                            {camera.camera_id}
                          </p>
                          <div className="flex items-center gap-4 mt-2 text-xs text-slate-500">
                            <span>Target FPS: {camera.target_fps}</span>
                            <span>•</span>
                            <span>Location: {camera.location || 'N/A'}</span>
                            {camera.motion_detection_enabled && (
                              <>
                                <span>•</span>
                                <Badge variant="outline" className="text-xs">Motion Detection</Badge>
                              </>
                            )}
                          </div>
                        </div>
                      </div>

                      <div className="flex items-center gap-2">
                        <Button
                          onClick={() => handleToggle(camera)}
                          variant={camera.is_running ? "destructive" : "default"}
                          size="sm"
                          disabled={!camera.enabled}
                        >
                          {camera.is_running ? (
                            <><Pause className="h-4 w-4 mr-2" />Stop</>
                          ) : (
                            <><Play className="h-4 w-4 mr-2" />Start</>
                          )}
                        </Button>

                        <Link href={`/cameras/${camera.camera_id}`}>
                          <Button variant="outline" size="sm">
                            <Eye className="h-4 w-4 mr-2" />
                            View
                          </Button>
                        </Link>

                        <Link href={`/cameras/${camera.camera_id}/edit`}>
                          <Button variant="outline" size="sm">
                            <Edit className="h-4 w-4" />
                          </Button>
                        </Link>

                        <Button
                          variant="outline"
                          size="sm"
                          onClick={() => handleDelete(camera.camera_id)}
                        >
                          <Trash2 className="h-4 w-4 text-red-500" />
                        </Button>
                      </div>
                    </div>

                    {camera.state === 'ERROR' && (
                      <div className="mt-4 rounded-md bg-red-50 dark:bg-red-900/20 p-3 text-sm text-red-600 dark:text-red-400">
                        ⚠️ Camera is in error state. Edit the configuration to fix any issues, then try starting it again.
                      </div>
                    )}
                  </CardContent>
                </Card>
              );
            })}
          </div>
        )}
      </main>
    </Layout>
  );
}
