'use client';

import { useEffect, useState } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Bell, Camera, RefreshCw, Search, Filter } from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { MotionEvent, Camera as CameraType } from '@/lib/types';

export default function EventsPage() {
  const [events, setEvents] = useState<MotionEvent[]>([]);
  const [cameras, setCameras] = useState<CameraType[]>([]);
  const [loading, setLoading] = useState(true);
  const [filterCamera, setFilterCamera] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');

  useEffect(() => {
    loadData();
  }, [filterCamera]);

  const loadData = async () => {
    try {
      setLoading(true);
      const [camerasData, eventsData] = await Promise.all([
        apiClient.getCameras(),
        apiClient.getMotionEvents({
          camera_id: filterCamera !== 'all' ? filterCamera : undefined,
          limit: 100
        })
      ]);
      setCameras(camerasData);
      setEvents(eventsData.events);
    } catch (error) {
      console.error('Failed to load data:', error);
    } finally {
      setLoading(false);
    }
  };

  const filteredEvents = events.filter(event => {
    if (searchTerm) {
      return event.camera_id.toLowerCase().includes(searchTerm.toLowerCase());
    }
    return true;
  });

  if (loading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header
        title="Motion Events"
        description="View motion detection events"
        onRefresh={loadData}
      />

      <main className="container mx-auto px-6 py-8">
        {/* Filters */}
        <Card className="mb-6">
          <CardContent className="p-6">
            <div className="flex items-center gap-4">
              <div className="flex-1 relative">
                <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-muted-foreground" />
                <Input
                  placeholder="Search by camera ID..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="pl-10"
                />
              </div>

              <Select value={filterCamera} onValueChange={setFilterCamera}>
                <SelectTrigger className="w-64">
                  <Filter className="h-4 w-4 mr-2" />
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="all">All Cameras</SelectItem>
                  {cameras.map(camera => (
                    <SelectItem key={camera.camera_id} value={camera.camera_id}>
                      {camera.name}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>

              <Button onClick={loadData} variant="outline">
                <RefreshCw className="h-4 w-4 mr-2" />
                Refresh
              </Button>
            </div>
          </CardContent>
        </Card>

        {/* Events List */}
        {filteredEvents.length === 0 ? (
          <Card>
            <CardContent className="flex flex-col items-center justify-center py-16">
              <Bell className="h-16 w-16 text-muted-foreground mb-4" />
              <h3 className="text-xl font-semibold mb-2">No Events Found</h3>
              <p className="text-sm text-muted-foreground">
                {filterCamera !== 'all'
                  ? 'No motion events for this camera'
                  : 'No motion events recorded yet'}
              </p>
            </CardContent>
          </Card>
        ) : (
          <div className="space-y-3">
            {filteredEvents.map((event, index) => {
              const camera = cameras.find(c => c.camera_id === event.camera_id);
              const eventDate = new Date(event.timestamp);

              return (
                <Card key={event.id || index} className="hover:shadow-md transition-shadow">
                  <CardContent className="p-4">
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-4 flex-1">
                        <div className="h-12 w-12 rounded-lg bg-orange-100 dark:bg-orange-900/20 flex items-center justify-center">
                          <Bell className="h-6 w-6 text-orange-600" />
                        </div>

                        <div className="flex-1">
                          <div className="flex items-center gap-2 mb-1">
                            <Camera className="h-4 w-4 text-muted-foreground" />
                            <span className="font-semibold text-slate-900 dark:text-white">
                              {camera?.name || event.camera_id}
                            </span>
                            <Badge variant="outline" className="ml-2">
                              Motion Detected
                            </Badge>
                          </div>

                          <div className="flex items-center gap-4 text-sm text-slate-600 dark:text-slate-400">
                            <span>
                              {eventDate.toLocaleString()}
                            </span>
                            <span>•</span>
                            <span>
                              Motion Area: {event.motion_area.toLocaleString()}px
                            </span>
                            <span>•</span>
                            <span>
                              Contours: {event.num_contours}
                            </span>
                            <span>•</span>
                            <span>
                              Confidence: {(event.confidence * 100).toFixed(1)}%
                            </span>
                          </div>
                        </div>
                      </div>

                      <div className="flex items-center gap-2">
                        <div className="text-right">
                          <div className="text-sm font-semibold text-slate-900 dark:text-white">
                            {(event.confidence * 100).toFixed(0)}%
                          </div>
                          <div className="text-xs text-muted-foreground">
                            Confidence
                          </div>
                        </div>
                      </div>
                    </div>

                    {event.bounding_boxes && event.bounding_boxes.length > 0 && (
                      <div className="mt-3 pt-3 border-t">
                        <div className="text-xs text-muted-foreground">
                          Bounding Boxes: {event.bounding_boxes.length} detected
                        </div>
                      </div>
                    )}
                  </CardContent>
                </Card>
              );
            })}
          </div>
        )}

        {filteredEvents.length > 0 && (
          <div className="mt-6 text-center">
            <p className="text-sm text-muted-foreground">
              Showing {filteredEvents.length} event{filteredEvents.length !== 1 ? 's' : ''}
            </p>
          </div>
        )}
      </main>
    </div>
  );
}
