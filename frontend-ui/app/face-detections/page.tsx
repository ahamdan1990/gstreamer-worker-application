"use client";

import { useState, useEffect } from "react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Alert, AlertDescription } from "@/components/ui/alert";
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from "@/components/ui/table";
import {
  User,
  Clock,
  Camera,
  CheckCircle2,
  XCircle,
  Search,
  Filter,
  RefreshCw,
  Eye,
  AlertCircle,
  TrendingUp,
  Users,
  Activity,
  Calendar,
  Image as ImageIcon
} from "lucide-react";
import { Layout } from "@/components/layout";
import wsClient from "@/lib/websocket";

const API_URL = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8002";

interface FaceDetectionEvent {
  id: string;
  tracking_id: string;
  camera_id: string;
  subject: string;
  is_recognized: boolean;
  confidence: number;
  recognition_attempts: number;
  tracking_duration_seconds: number;
  is_first_detection: boolean;
  image_url: string | null;
  thumbnail_url: string | null;
  bounding_box: any;
  extra_metadata: any;
  worker_metadata: any;
  created_at: string;
  updated_at: string;
}

interface DailySummaryItem {
  subject: string;
  date: string;
  detection_count: number;
  first_seen: string;
  last_seen: string;
  avg_confidence: number;
  sample_images: string[];
  cameras: string[];
  total_duration: number;
}

interface Statistics {
  total: number;
  recognized: number;
  unknown: number;
  avgDuration: number;
  avgAttempts: number;
}

export default function FaceDetectionsPage() {
  const [events, setEvents] = useState<FaceDetectionEvent[]>([]);
  const [dailySummary, setDailySummary] = useState<DailySummaryItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [searchTerm, setSearchTerm] = useState("");
  const [filterRecognized, setFilterRecognized] = useState<string>("all");
  const [selectedCamera, setSelectedCamera] = useState<string>("all");
  const [cameras, setCameras] = useState<string[]>([]);
  const [statistics, setStatistics] = useState<Statistics | null>(null);
  const [page, setPage] = useState(0);
  const [totalEvents, setTotalEvents] = useState(0);
  const [activeTab, setActiveTab] = useState("table");
  const [wsConnected, setWsConnected] = useState(false);
  const limit = 50;

  // WebSocket connection status
  useEffect(() => {
    const interval = setInterval(() => setWsConnected(wsClient.isConnected()), 1000);
    return () => clearInterval(interval);
  }, []);

  // Fetch face detection events
  const fetchEvents = async () => {
    try {
      setLoading(true);
      setError(null);

      let url = `${API_URL}/api/v1/events/faces?skip=${page * limit}&limit=${limit}`;

      if (filterRecognized !== "all") {
        url += `&is_recognized=${filterRecognized === "recognized"}`;
      }

      if (selectedCamera !== "all") {
        url += `&camera_id=${encodeURIComponent(selectedCamera)}`;
      }

      const response = await fetch(url);
      if (!response.ok) throw new Error("Failed to fetch events");

      const data = await response.json();
      setEvents(data.events || []);
      setTotalEvents(data.total || 0);

      // Extract unique cameras
      const uniqueCameras = Array.from(
        new Set(data.events.map((e: FaceDetectionEvent) => e.camera_id))
      ) as string[];
      setCameras(uniqueCameras);

      // Calculate statistics
      calculateStatistics(data.events);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load events");
    } finally {
      setLoading(false);
    }
  };

  // Fetch daily summary
  const fetchDailySummary = async () => {
    try {
      setLoading(true);
      setError(null);

      let url = `${API_URL}/api/v1/events/faces/daily-summary`;
      const params = [];

      if (selectedCamera !== "all") {
        params.push(`camera_id=${encodeURIComponent(selectedCamera)}`);
      }

      if (params.length > 0) {
        url += `?${params.join("&")}`;
      }

      const response = await fetch(url);
      if (!response.ok) throw new Error("Failed to fetch daily summary");

      const data = await response.json();
      setDailySummary(data.summary || []);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load daily summary");
    } finally {
      setLoading(false);
    }
  };

  const calculateStatistics = (eventList: FaceDetectionEvent[]) => {
    if (eventList.length === 0) {
      setStatistics(null);
      return;
    }

    const recognized = eventList.filter(e => e.is_recognized).length;
    const unknown = eventList.length - recognized;
    const avgDuration = eventList.reduce((sum, e) => sum + e.tracking_duration_seconds, 0) / eventList.length;
    const avgAttempts = eventList.reduce((sum, e) => sum + e.recognition_attempts, 0) / eventList.length;

    setStatistics({
      total: eventList.length,
      recognized,
      unknown,
      avgDuration,
      avgAttempts
    });
  };

  useEffect(() => {
    if (activeTab === "table") {
      fetchEvents();
    } else if (activeTab === "daily") {
      fetchDailySummary();
    }
  }, [page, filterRecognized, selectedCamera, activeTab]);

  // Filter events based on search term
  const filteredEvents = events.filter(event => {
    if (!searchTerm) return true;
    const term = searchTerm.toLowerCase();
    return (
      event.tracking_id.toLowerCase().includes(term) ||
      event.subject.toLowerCase().includes(term) ||
      event.camera_id.toLowerCase().includes(term)
    );
  });

  const formatDuration = (seconds: number) => {
    if (seconds < 60) return `${seconds.toFixed(1)}s`;
    const minutes = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${minutes}m ${secs}s`;
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleString();
  };

  const formatTime = (dateString: string) => {
    return new Date(dateString).toLocaleTimeString();
  };

  const getStatusBadge = (event: FaceDetectionEvent) => {
    if (event.is_recognized) {
      return (
        <Badge className="bg-green-500 hover:bg-green-600">
          <CheckCircle2 className="w-3 h-3 mr-1" />
          Recognized
        </Badge>
      );
    }

    if (event.recognition_attempts >= 3) {
      return (
        <Badge variant="destructive">
          <XCircle className="w-3 h-3 mr-1" />
          Unknown
        </Badge>
      );
    }

    return (
      <Badge variant="secondary">
        <Activity className="w-3 h-3 mr-1" />
        Tracking ({event.recognition_attempts}/3)
      </Badge>
    );
  };

  return (
    <Layout wsConnected={wsConnected}>
      <div className="container mx-auto py-6 space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold tracking-tight">Face Detections</h1>
          <p className="text-muted-foreground mt-1">
            Real-time face detection tracking with recognition status
          </p>
        </div>
        <Button onClick={activeTab === "table" ? fetchEvents : fetchDailySummary} variant="outline" size="sm">
          <RefreshCw className="w-4 h-4 mr-2" />
          Refresh
        </Button>
      </div>

      {/* Statistics Cards */}
      {statistics && activeTab === "table" && (
        <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-5">
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Total Detections</CardTitle>
              <Eye className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{statistics.total}</div>
              <p className="text-xs text-muted-foreground">
                Showing {filteredEvents.length} results
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Recognized</CardTitle>
              <CheckCircle2 className="h-4 w-4 text-green-500" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{statistics.recognized}</div>
              <p className="text-xs text-muted-foreground">
                {((statistics.recognized / statistics.total) * 100).toFixed(1)}% success rate
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Unknown</CardTitle>
              <Users className="h-4 w-4 text-orange-500" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{statistics.unknown}</div>
              <p className="text-xs text-muted-foreground">
                Unrecognized faces
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Avg Duration</CardTitle>
              <Clock className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{formatDuration(statistics.avgDuration)}</div>
              <p className="text-xs text-muted-foreground">
                Per detection session
              </p>
            </CardContent>
          </Card>

          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Avg Attempts</CardTitle>
              <TrendingUp className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{statistics.avgAttempts.toFixed(1)}</div>
              <p className="text-xs text-muted-foreground">
                Recognition attempts
              </p>
            </CardContent>
          </Card>
        </div>
      )}

      {/* Filters */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg flex items-center gap-2">
            <Filter className="w-5 h-5" />
            Filters & Search
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid gap-4 md:grid-cols-3">
            <div className="space-y-2">
              <label className="text-sm font-medium">Search</label>
              <div className="relative">
                <Search className="absolute left-2 top-2.5 h-4 w-4 text-muted-foreground" />
                <Input
                  placeholder="Search by tracking ID, subject, camera..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="pl-8"
                />
              </div>
            </div>

            {activeTab === "table" && (
              <div className="space-y-2">
                <label className="text-sm font-medium">Recognition Status</label>
                <Select value={filterRecognized} onValueChange={setFilterRecognized}>
                  <SelectTrigger>
                    <SelectValue placeholder="All statuses" />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="all">All Statuses</SelectItem>
                    <SelectItem value="recognized">Recognized Only</SelectItem>
                    <SelectItem value="unknown">Unknown Only</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            )}

            <div className="space-y-2">
              <label className="text-sm font-medium">Camera</label>
              <Select value={selectedCamera} onValueChange={setSelectedCamera}>
                <SelectTrigger>
                  <SelectValue placeholder="All cameras" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="all">All Cameras</SelectItem>
                  {cameras.map(camera => (
                    <SelectItem key={camera} value={camera}>{camera}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Error Alert */}
      {error && (
        <Alert variant="destructive">
          <AlertCircle className="h-4 w-4" />
          <AlertDescription>{error}</AlertDescription>
        </Alert>
      )}

      {/* Tabs */}
      <Tabs value={activeTab} onValueChange={setActiveTab} className="w-full">
        <TabsList className="grid w-full grid-cols-2 lg:w-[400px]">
          <TabsTrigger value="table">All Detections</TabsTrigger>
          <TabsTrigger value="daily">Daily Summary</TabsTrigger>
        </TabsList>

        {/* Tab 1: All Detections Table */}
        <TabsContent value="table" className="space-y-4">
          {loading ? (
            <div className="flex items-center justify-center py-12">
              <RefreshCw className="w-8 h-8 animate-spin text-muted-foreground" />
            </div>
          ) : filteredEvents.length === 0 ? (
            <Card>
              <CardContent className="py-12">
                <div className="text-center text-muted-foreground">
                  <Eye className="w-12 h-12 mx-auto mb-4 opacity-20" />
                  <p className="text-lg font-medium">No face detections found</p>
                  <p className="text-sm mt-1">Try adjusting your filters or wait for new detections</p>
                </div>
              </CardContent>
            </Card>
          ) : (
            <>
              <Card>
                <CardContent className="p-0">
                  <Table>
                    <TableHeader>
                      <TableRow>
                        <TableHead>Face</TableHead>
                        <TableHead>Subject</TableHead>
                        <TableHead>Status</TableHead>
                        <TableHead>Camera</TableHead>
                        <TableHead>Confidence</TableHead>
                        <TableHead>Duration</TableHead>
                        <TableHead>Attempts</TableHead>
                        <TableHead>Detected At</TableHead>
                      </TableRow>
                    </TableHeader>
                    <TableBody>
                      {filteredEvents.map((event) => (
                        <TableRow key={event.id}>
                          <TableCell>
                            {event.image_url ? (
                              <img
                                src={`${API_URL}${event.image_url}`}
                                alt="Face"
                                className="w-16 h-16 rounded-lg object-cover"
                                onError={(e) => {
                                  e.currentTarget.src = "/placeholder-face.png";
                                }}
                              />
                            ) : (
                              <div className="w-16 h-16 rounded-lg bg-muted flex items-center justify-center">
                                <ImageIcon className="w-6 h-6 text-muted-foreground" />
                              </div>
                            )}
                          </TableCell>
                          <TableCell>
                            <div className="flex items-center gap-2">
                              {event.is_recognized ? (
                                <>
                                  <User className="w-4 h-4 text-green-500" />
                                  <span className="font-medium">{event.subject}</span>
                                </>
                              ) : (
                                <>
                                  <User className="w-4 h-4 text-orange-500" />
                                  <span className="text-muted-foreground">Unknown</span>
                                </>
                              )}
                            </div>
                          </TableCell>
                          <TableCell>{getStatusBadge(event)}</TableCell>
                          <TableCell>
                            <div className="flex items-center gap-2">
                              <Camera className="w-4 h-4 text-muted-foreground" />
                              {event.camera_id}
                            </div>
                          </TableCell>
                          <TableCell>
                            <Badge variant="secondary">
                              {(event.confidence * 100).toFixed(0)}%
                            </Badge>
                          </TableCell>
                          <TableCell>{formatDuration(event.tracking_duration_seconds)}</TableCell>
                          <TableCell>{event.recognition_attempts}/3</TableCell>
                          <TableCell className="text-sm text-muted-foreground">
                            {formatDate(event.created_at)}
                          </TableCell>
                        </TableRow>
                      ))}
                    </TableBody>
                  </Table>
                </CardContent>
              </Card>

              {/* Pagination */}
              <Card>
                <CardContent className="py-4">
                  <div className="flex items-center justify-between">
                    <div className="text-sm text-muted-foreground">
                      Showing {page * limit + 1}-{Math.min((page + 1) * limit, totalEvents)} of {totalEvents} events
                    </div>
                    <div className="flex gap-2">
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => setPage(p => Math.max(0, p - 1))}
                        disabled={page === 0}
                      >
                        Previous
                      </Button>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => setPage(p => p + 1)}
                        disabled={(page + 1) * limit >= totalEvents}
                      >
                        Next
                      </Button>
                    </div>
                  </div>
                </CardContent>
              </Card>
            </>
          )}
        </TabsContent>

        {/* Tab 2: Daily Summary */}
        <TabsContent value="daily" className="space-y-4">
          {loading ? (
            <div className="flex items-center justify-center py-12">
              <RefreshCw className="w-8 h-8 animate-spin text-muted-foreground" />
            </div>
          ) : dailySummary.length === 0 ? (
            <Card>
              <CardContent className="py-12">
                <div className="text-center text-muted-foreground">
                  <Calendar className="w-12 h-12 mx-auto mb-4 opacity-20" />
                  <p className="text-lg font-medium">No recognized persons found</p>
                  <p className="text-sm mt-1">Daily summary shows only recognized faces</p>
                </div>
              </CardContent>
            </Card>
          ) : (
            <div className="space-y-4">
              {dailySummary.map((item, index) => (
                <Card key={index} className="overflow-hidden">
                  <CardHeader className="bg-gradient-to-r from-blue-50 to-purple-50 dark:from-blue-900/20 dark:to-purple-900/20">
                    <div className="flex items-start justify-between">
                      <div>
                        <CardTitle className="flex items-center gap-2">
                          <User className="w-5 h-5 text-blue-600" />
                          {item.subject}
                        </CardTitle>
                        <CardDescription className="mt-1">
                          <Calendar className="w-4 h-4 inline mr-1" />
                          {new Date(item.date).toLocaleDateString('en-US', {
                            weekday: 'long',
                            year: 'numeric',
                            month: 'long',
                            day: 'numeric'
                          })}
                        </CardDescription>
                      </div>
                      <Badge className="bg-blue-600">
                        {item.detection_count} detection{item.detection_count !== 1 ? 's' : ''}
                      </Badge>
                    </div>
                  </CardHeader>
                  <CardContent className="pt-6">
                    <div className="grid gap-4 md:grid-cols-2">
                      {/* Statistics */}
                      <div className="space-y-3">
                        <div className="flex items-center justify-between text-sm">
                          <span className="text-muted-foreground flex items-center gap-2">
                            <Clock className="w-4 h-4" />
                            First Seen
                          </span>
                          <span className="font-medium">{formatTime(item.first_seen)}</span>
                        </div>
                        <div className="flex items-center justify-between text-sm">
                          <span className="text-muted-foreground flex items-center gap-2">
                            <Clock className="w-4 h-4" />
                            Last Seen
                          </span>
                          <span className="font-medium">{formatTime(item.last_seen)}</span>
                        </div>
                        <div className="flex items-center justify-between text-sm">
                          <span className="text-muted-foreground flex items-center gap-2">
                            <TrendingUp className="w-4 h-4" />
                            Avg Confidence
                          </span>
                          <Badge variant="secondary">{(item.avg_confidence * 100).toFixed(0)}%</Badge>
                        </div>
                        <div className="flex items-center justify-between text-sm">
                          <span className="text-muted-foreground flex items-center gap-2">
                            <Camera className="w-4 h-4" />
                            Cameras
                          </span>
                          <span className="font-medium">{item.cameras.join(', ')}</span>
                        </div>
                        <div className="flex items-center justify-between text-sm">
                          <span className="text-muted-foreground flex items-center gap-2">
                            <Activity className="w-4 h-4" />
                            Total Duration
                          </span>
                          <span className="font-medium">{formatDuration(item.total_duration)}</span>
                        </div>
                      </div>

                      {/* Sample Images */}
                      <div>
                        <p className="text-sm font-medium mb-2">Sample Images</p>
                        <div className="grid grid-cols-3 gap-2">
                          {item.sample_images.map((img, idx) => (
                            <img
                              key={idx}
                              src={`${API_URL}${img}`}
                              alt={`Sample ${idx + 1}`}
                              className="w-full h-24 rounded-lg object-cover border-2 border-border"
                              onError={(e) => {
                                e.currentTarget.src = "/placeholder-face.png";
                              }}
                            />
                          ))}
                          {item.sample_images.length === 0 && (
                            <div className="col-span-3 text-center text-sm text-muted-foreground py-4">
                              No images available
                            </div>
                          )}
                        </div>
                      </div>
                    </div>
                  </CardContent>
                </Card>
              ))}
            </div>
          )}
        </TabsContent>
      </Tabs>
      </div>
    </Layout>
  );
}
