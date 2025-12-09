'use client';

import { useEffect, useState } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import {
  Bell,
  Camera as CameraIcon,
  RefreshCw,
  Search,
  Filter,
  User,
  Clock,
  CheckCircle,
  XCircle,
  Activity,
  TrendingUp,
} from 'lucide-react';
import { Layout } from '@/components/layout';
import apiClient from '@/lib/api';
import wsClient from '@/lib/websocket';
import type { PersonEvent, Camera as CameraType } from '@/lib/types';

export default function EventsPage() {
  const [events, setEvents] = useState<PersonEvent[]>([]);
  const [cameras, setCameras] = useState<CameraType[]>([]);
  const [loading, setLoading] = useState(true);
  const [filterCamera, setFilterCamera] = useState<string>('all');
  const [filterAcknowledged, setFilterAcknowledged] = useState<boolean | undefined>(undefined);
  const [searchTerm, setSearchTerm] = useState('');
  const [wsConnected, setWsConnected] = useState(false);

  useEffect(() => {
    loadData();
    const interval = setInterval(() => setWsConnected(wsClient.isConnected()), 1000);

    wsClient.connect();

    // Subscribe to person recognition events for real-time updates
    const unsubscribe = wsClient.subscribeToPersonRecognized((event) => {
      // Add new event to the top of the list
      setEvents((prev) => [
        {
          id: `${Date.now()}`, // Temporary ID until we reload
          profile_id: event.profile_id,
          camera_id: event.camera_id,
          subject: event.subject,
          similarity: event.similarity,
          detection_confidence: event.detection_confidence || 0,
          is_new_person: event.is_new_person,
          is_new_at_camera: event.is_new_at_camera || false,
          dwell_time_seconds: event.dwell_time_seconds || 0,
          other_cameras: event.other_cameras || [],
          image_url: event.image_url,
          thumbnail_url: event.thumbnail_url,
          bounding_box: event.bounding_box,
          alert_sent: false,
          acknowledged: false,
          created_at: event.created_at || new Date().toISOString(),
        },
        ...prev.slice(0, 99), // Keep only last 100 events
      ]);
    });

    return () => {
      unsubscribe();
      clearInterval(interval);
    };
  }, []);

  const loadData = async () => {
    try {
      setLoading(true);
      const [camerasData, eventsData] = await Promise.all([
        apiClient.getCameras(),
        apiClient.getPersonEvents({
          camera_id: filterCamera !== 'all' ? filterCamera : undefined,
          acknowledged: filterAcknowledged,
          limit: 100,
        }),
      ]);
      setCameras(camerasData);
      setEvents(eventsData.events);
    } catch (error) {
      console.error('Failed to load data:', error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const debounce = setTimeout(() => {
      loadData();
    }, 300);
    return () => clearTimeout(debounce);
  }, [filterCamera, filterAcknowledged]);

  const handleAcknowledge = async (eventId: string) => {
    try {
      await apiClient.acknowledgePersonEvent(eventId);
      setEvents((prev) =>
        prev.map((e) => (e.id === eventId ? { ...e, acknowledged: true } : e))
      );
    } catch (error) {
      console.error('Failed to acknowledge event:', error);
      alert('Failed to acknowledge event');
    }
  };

  const filteredEvents = events.filter((event) => {
    if (searchTerm) {
      const query = searchTerm.toLowerCase();
      return (
        event.camera_id.toLowerCase().includes(query) ||
        event.subject.toLowerCase().includes(query) ||
        event.profile?.name?.toLowerCase().includes(query)
      );
    }
    return true;
  });

  // Calculate statistics
  const totalEvents = events.length;
  const newPersonEvents = events.filter((e) => e.is_new_person).length;
  const acknowledgedEvents = events.filter((e) => e.acknowledged).length;
  const alertsSent = events.filter((e) => e.alert_sent).length;

  if (loading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  return (
    <Layout wsConnected={wsConnected}>
      <div className="container mx-auto py-8 px-4">
        {/* Page Header */}
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-2">
              <Activity className="h-8 w-8 text-blue-600" />
              Person Events
            </h1>
            <p className="text-gray-500 mt-1">
              Face detection and recognition events from all cameras
            </p>
          </div>
          <div className="flex items-center gap-2">
            <Badge variant={wsConnected ? 'default' : 'secondary'} className="bg-green-500">
              {wsConnected ? '● Live' : '○ Disconnected'}
            </Badge>
            <Button onClick={loadData} variant="outline">
              <RefreshCw className="h-4 w-4 mr-2" />
              Refresh
            </Button>
          </div>
        </div>

        {/* Statistics Cards */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Total Events</p>
                  <p className="text-2xl font-bold">{totalEvents}</p>
                </div>
                <Activity className="h-8 w-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">New Persons</p>
                  <p className="text-2xl font-bold">{newPersonEvents}</p>
                </div>
                <User className="h-8 w-8 text-purple-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Acknowledged</p>
                  <p className="text-2xl font-bold">{acknowledgedEvents}</p>
                </div>
                <CheckCircle className="h-8 w-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Alerts Sent</p>
                  <p className="text-2xl font-bold">{alertsSent}</p>
                </div>
                <Bell className="h-8 w-8 text-orange-500" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Filters */}
        <Card className="mb-6">
          <CardContent className="p-6">
            <div className="flex flex-col md:flex-row items-center gap-4">
              <div className="flex-1 relative w-full">
                <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-gray-400" />
                <Input
                  placeholder="Search by camera, subject, or profile name..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="pl-10"
                />
              </div>

              <Select value={filterCamera} onValueChange={setFilterCamera}>
                <SelectTrigger className="w-full md:w-64">
                  <Filter className="h-4 w-4 mr-2" />
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="all">All Cameras</SelectItem>
                  {cameras.map((camera) => (
                    <SelectItem key={camera.camera_id} value={camera.camera_id}>
                      {camera.name}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>

              <div className="flex gap-2">
                <Button
                  variant={filterAcknowledged === undefined ? 'default' : 'outline'}
                  onClick={() => setFilterAcknowledged(undefined)}
                  size="sm"
                >
                  All
                </Button>
                <Button
                  variant={filterAcknowledged === false ? 'default' : 'outline'}
                  onClick={() => setFilterAcknowledged(false)}
                  size="sm"
                >
                  Unacknowledged
                </Button>
                <Button
                  variant={filterAcknowledged === true ? 'default' : 'outline'}
                  onClick={() => setFilterAcknowledged(true)}
                  size="sm"
                >
                  Acknowledged
                </Button>
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Events List */}
        {filteredEvents.length === 0 ? (
          <Card>
            <CardContent className="py-12">
              <div className="text-center">
                <Activity className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                <h3 className="text-lg font-semibold mb-2">No events found</h3>
                <p className="text-gray-500">
                  {searchTerm
                    ? 'Try adjusting your search query or filters'
                    : 'Person recognition events will appear here'}
                </p>
              </div>
            </CardContent>
          </Card>
        ) : (
          <div className="space-y-4">
            {filteredEvents.map((event) => {
              // Clean up image URL: remove ./ prefix and ensure it starts with /
              const cleanImageUrl = event.image_url ? event.image_url.replace(/^\.\//, '/').replace(/^(?!\/)/, '/') : null;

              return (
              <Card key={event.id} className="hover:shadow-md transition-shadow">
                <CardContent className="pt-6">
                  <div className="flex items-start gap-4">
                    {/* Face Crop Image or Event Icon */}
                    <div className="flex-shrink-0">
                      {cleanImageUrl ? (
                        <div className="w-20 h-20 rounded-lg overflow-hidden border-2 border-gray-200 bg-gray-50">
                          <img
                            src={`http://192.168.0.24:8002${cleanImageUrl}`}
                            alt={`Face crop for ${event.subject}`}
                            className="w-full h-full object-cover"
                            onError={(e) => {
                              // Fallback to icon if image fails to load
                              const target = e.currentTarget as HTMLImageElement;
                              target.style.display = 'none';
                              const parent = target.parentElement;
                              if (parent) {
                                const fallbackDiv = document.createElement('div');
                                fallbackDiv.className = `w-20 h-20 rounded-lg flex items-center justify-center ${
                                  event.is_new_person
                                    ? 'bg-purple-100 text-purple-600'
                                    : 'bg-blue-100 text-blue-600'
                                }`;
                                fallbackDiv.innerHTML = '<svg class="h-10 w-10" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M16 7a4 4 0 11-8 0 4 4 0 018 0zM12 14a7 7 0 00-7 7h14a7 7 0 00-7-7z" /></svg>';
                                parent.appendChild(fallbackDiv);
                              }
                            }}
                          />
                        </div>
                      ) : (
                        <div
                          className={`w-20 h-20 rounded-lg flex items-center justify-center ${
                            event.is_new_person
                              ? 'bg-purple-100 text-purple-600'
                              : 'bg-blue-100 text-blue-600'
                          }`}
                        >
                          <User className="h-10 w-10" />
                        </div>
                      )}
                    </div>

                    {/* Event Details */}
                    <div className="flex-1">
                      <div className="flex items-start justify-between mb-2">
                        <div>
                          <h3 className="font-semibold text-lg">
                            {event.profile?.name || event.subject}
                          </h3>
                          <p className="text-sm text-gray-500">Subject: {event.subject}</p>
                        </div>
                        <div className="flex flex-col items-end gap-2">
                          {event.is_new_person && (
                            <Badge variant="default" className="bg-purple-500">
                              New Person
                            </Badge>
                          )}
                          {event.is_new_at_camera && (
                            <Badge variant="default" className="bg-blue-500">
                              New at Camera
                            </Badge>
                          )}
                          {event.acknowledged ? (
                            <Badge variant="default" className="bg-green-500">
                              ✓ Acknowledged
                            </Badge>
                          ) : (
                            <Badge variant="secondary">Unacknowledged</Badge>
                          )}
                        </div>
                      </div>

                      <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-3">
                        <div>
                          <p className="text-xs text-gray-500">Camera</p>
                          <p className="font-semibold text-sm flex items-center gap-1">
                            <CameraIcon className="h-3 w-3" />
                            {event.camera_id}
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Similarity</p>
                          <p className="font-semibold text-sm">
                            {(event.similarity * 100).toFixed(1)}%
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Detection</p>
                          <p className="font-semibold text-sm">
                            {(event.detection_confidence * 100).toFixed(1)}%
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Dwell Time</p>
                          <p className="font-semibold text-sm">
                            {event.dwell_time_seconds}s
                          </p>
                        </div>
                      </div>

                      {event.other_cameras.length > 0 && (
                        <div className="mb-3">
                          <p className="text-xs text-gray-500 mb-1">Also seen on:</p>
                          <div className="flex flex-wrap gap-1">
                            {event.other_cameras.map((cam) => (
                              <Badge key={cam} variant="outline" className="text-xs">
                                {cam}
                              </Badge>
                            ))}
                          </div>
                        </div>
                      )}

                      <div className="flex items-center justify-between">
                        <div className="flex items-center gap-2 text-xs text-gray-500">
                          <Clock className="h-3 w-3" />
                          {new Date(event.created_at).toLocaleString()}
                        </div>
                        <div className="flex gap-2">
                          {event.alert_sent && (
                            <Badge variant="outline" className="text-xs">
                              Alert Sent
                            </Badge>
                          )}
                          {!event.acknowledged && (
                            <Button
                              size="sm"
                              variant="outline"
                              onClick={() => handleAcknowledge(event.id)}
                            >
                              <CheckCircle className="h-4 w-4 mr-2" />
                              Acknowledge
                            </Button>
                          )}
                        </div>
                      </div>
                    </div>
                  </div>
                </CardContent>
              </Card>
              );
            })}
          </div>
        )}
      </div>
    </Layout>
  );
}
