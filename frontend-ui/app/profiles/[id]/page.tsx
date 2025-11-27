'use client';

import { useEffect, useState } from 'react';
import { useRouter, useParams } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import {
  ArrowLeft,
  Edit,
  Trash2,
  User,
  Mail,
  Phone,
  Activity,
  Clock,
  Camera,
  Bell,
  BellOff,
  Shield,
  Image as ImageIcon,
  RefreshCw,
} from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Profile, PersonEvent } from '@/lib/types';

export default function ProfileDetailPage() {
  const router = useRouter();
  const params = useParams();
  const profileId = params.id as string;

  const [profile, setProfile] = useState<Profile | null>(null);
  const [recentEvents, setRecentEvents] = useState<PersonEvent[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadProfile();
    loadRecentEvents();
  }, [profileId]);

  const loadProfile = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getProfile(profileId);
      setProfile(data);
    } catch (error) {
      console.error('Failed to load profile:', error);
      alert('Failed to load profile');
      router.push('/profiles');
    } finally {
      setLoading(false);
    }
  };

  const loadRecentEvents = async () => {
    try {
      const response = await fetch(
        `http://localhost:8002/api/v1/events/persons?profile_id=${profileId}&limit=10`
      );
      if (response.ok) {
        const data = await response.json();
        setRecentEvents(data.events || []);
      }
    } catch (error) {
      console.error('Failed to load recent events:', error);
    }
  };

  const handleDelete = async () => {
    if (!confirm('Are you sure you want to delete this profile? This action cannot be undone.')) {
      return;
    }

    try {
      await apiClient.deleteProfile(profileId);
      router.push('/profiles');
    } catch (error) {
      console.error('Failed to delete profile:', error);
      alert('Failed to delete profile');
    }
  };

  const handleToggleActive = async () => {
    if (!profile) return;

    try {
      await apiClient.updateProfile(profileId, {
        is_active: !profile.is_active,
      });
      loadProfile();
    } catch (error) {
      console.error('Failed to update profile:', error);
    }
  };

  const formatDate = (dateStr?: string) => {
    if (!dateStr) return 'Never';
    const date = new Date(dateStr);
    return date.toLocaleString();
  };

  const formatRelativeTime = (dateStr?: string) => {
    if (!dateStr) return 'Never';
    const date = new Date(dateStr);
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffMins = Math.floor(diffMs / 60000);
    const diffHours = Math.floor(diffMs / 3600000);
    const diffDays = Math.floor(diffMs / 86400000);

    if (diffMins < 1) return 'Just now';
    if (diffMins < 60) return `${diffMins}m ago`;
    if (diffHours < 24) return `${diffHours}h ago`;
    if (diffDays < 7) return `${diffDays}d ago`;
    return date.toLocaleDateString();
  };

  if (loading || !profile) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900">
      <Header />

      <main className="container mx-auto px-4 py-8">
        <div className="mb-6">
          <Link href="/profiles">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Profiles
            </Button>
          </Link>
        </div>

        {/* Header */}
        <div className="flex items-start justify-between mb-8">
          <div className="flex items-start gap-6">
            <div className="w-24 h-24 rounded-full bg-gradient-to-br from-blue-500 to-purple-600 flex items-center justify-center text-white font-bold text-4xl">
              {profile.name.charAt(0).toUpperCase()}
            </div>
            <div>
              <div className="flex items-center gap-3 mb-2">
                <h1 className="text-3xl font-bold text-gray-900 dark:text-white">
                  {profile.name}
                </h1>
                {profile.is_active ? (
                  <Badge variant="default" className="text-sm">
                    Active
                  </Badge>
                ) : (
                  <Badge variant="secondary" className="text-sm">
                    Inactive
                  </Badge>
                )}
                {profile.auto_created && (
                  <Badge variant="outline" className="text-sm">
                    Auto-Created
                  </Badge>
                )}
              </div>
              <p className="text-sm text-gray-600 dark:text-gray-400 mb-1">
                Subject ID: <code className="text-xs bg-gray-100 dark:bg-gray-800 px-2 py-1 rounded">
                  {profile.compreface_subject_id}
                </code>
              </p>
              <p className="text-sm text-gray-600 dark:text-gray-400">
                Created {formatDate(profile.created_at)}
              </p>
            </div>
          </div>

          <div className="flex gap-2">
            <Button onClick={handleToggleActive} variant="outline">
              {profile.is_active ? (
                <>
                  <BellOff className="h-4 w-4 mr-2" />
                  Deactivate
                </>
              ) : (
                <>
                  <Bell className="h-4 w-4 mr-2" />
                  Activate
                </>
              )}
            </Button>
            <Link href={`/profiles/${profileId}/edit`}>
              <Button variant="outline">
                <Edit className="h-4 w-4 mr-2" />
                Edit
              </Button>
            </Link>
            <Button variant="outline" onClick={handleDelete}>
              <Trash2 className="h-4 w-4 mr-2 text-red-500" />
              Delete
            </Button>
          </div>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Left Column */}
          <div className="lg:col-span-2 space-y-6">
            {/* Statistics */}
            <div className="grid grid-cols-3 gap-4">
              <Card>
                <CardContent className="pt-6">
                  <div className="text-center">
                    <p className="text-3xl font-bold text-blue-600">{profile.total_detections}</p>
                    <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">Detections</p>
                  </div>
                </CardContent>
              </Card>

              <Card>
                <CardContent className="pt-6">
                  <div className="text-center">
                    <p className="text-3xl font-bold text-green-600">
                      {(profile.avg_recognition_similarity * 100).toFixed(0)}%
                    </p>
                    <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">Avg Similarity</p>
                  </div>
                </CardContent>
              </Card>

              <Card>
                <CardContent className="pt-6">
                  <div className="text-center">
                    <p className="text-2xl font-bold text-purple-600">
                      {profile.images?.length || 0}
                    </p>
                    <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">Images</p>
                  </div>
                </CardContent>
              </Card>
            </div>

            {/* Description */}
            {profile.description && (
              <Card>
                <CardHeader>
                  <CardTitle>Description</CardTitle>
                </CardHeader>
                <CardContent>
                  <p className="text-gray-700 dark:text-gray-300">{profile.description}</p>
                </CardContent>
              </Card>
            )}

            {/* Face Images */}
            <Card>
              <CardHeader>
                <CardTitle>Face Images</CardTitle>
                <CardDescription>Images used for face recognition</CardDescription>
              </CardHeader>
              <CardContent>
                {profile.images && profile.images.length > 0 ? (
                  <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                    {profile.images.map((imageUrl, index) => (
                      <div key={index} className="aspect-square rounded-lg overflow-hidden border-2 border-gray-200 dark:border-gray-700">
                        <img
                          src={imageUrl}
                          alt={`Face ${index + 1}`}
                          className="w-full h-full object-cover"
                        />
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className="text-center py-12 border-2 border-dashed border-gray-300 dark:border-gray-700 rounded-lg">
                    <ImageIcon className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                    <p className="text-gray-600 dark:text-gray-400">No images uploaded</p>
                  </div>
                )}
              </CardContent>
            </Card>

            {/* Recent Detections */}
            <Card>
              <CardHeader>
                <CardTitle>Recent Detections</CardTitle>
                <CardDescription>Latest recognition events</CardDescription>
              </CardHeader>
              <CardContent>
                {recentEvents.length > 0 ? (
                  <div className="space-y-4">
                    {recentEvents.map((event) => (
                      <div
                        key={event.id}
                        className="flex items-center justify-between p-4 border rounded-lg hover:bg-gray-50 dark:hover:bg-gray-800"
                      >
                        <div className="flex items-center gap-4">
                          {event.thumbnail_url && (
                            <img
                              src={event.thumbnail_url}
                              alt="Detection"
                              className="w-12 h-12 rounded object-cover"
                            />
                          )}
                          <div>
                            <div className="flex items-center gap-2">
                              <Camera className="h-4 w-4 text-gray-400" />
                              <span className="font-medium">{event.camera_id}</span>
                            </div>
                            <div className="flex items-center gap-2 text-sm text-gray-600 dark:text-gray-400">
                              <Clock className="h-3 w-3" />
                              <span>{formatRelativeTime(event.created_at)}</span>
                            </div>
                          </div>
                        </div>
                        <div className="text-right">
                          <div className="text-sm font-medium">
                            {(event.similarity * 100).toFixed(1)}% match
                          </div>
                          <div className="text-xs text-gray-600 dark:text-gray-400">
                            {(event.detection_confidence * 100).toFixed(1)}% confidence
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className="text-center py-8">
                    <Activity className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                    <p className="text-gray-600 dark:text-gray-400">No recent detections</p>
                  </div>
                )}
              </CardContent>
            </Card>
          </div>

          {/* Right Column */}
          <div className="space-y-6">
            {/* Contact Information */}
            <Card>
              <CardHeader>
                <CardTitle>Contact Information</CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                {profile.email ? (
                  <div className="flex items-center gap-3">
                    <Mail className="h-4 w-4 text-gray-400" />
                    <span className="text-sm">{profile.email}</span>
                  </div>
                ) : (
                  <div className="flex items-center gap-3 text-gray-400">
                    <Mail className="h-4 w-4" />
                    <span className="text-sm">No email</span>
                  </div>
                )}

                {profile.phone ? (
                  <div className="flex items-center gap-3">
                    <Phone className="h-4 w-4 text-gray-400" />
                    <span className="text-sm">{profile.phone}</span>
                  </div>
                ) : (
                  <div className="flex items-center gap-3 text-gray-400">
                    <Phone className="h-4 w-4" />
                    <span className="text-sm">No phone</span>
                  </div>
                )}
              </CardContent>
            </Card>

            {/* Alert Settings */}
            <Card>
              <CardHeader>
                <CardTitle>Alert Settings</CardTitle>
              </CardHeader>
              <CardContent>
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    {profile.alert_on_recognition ? (
                      <>
                        <Bell className="h-4 w-4 text-orange-500" />
                        <span className="text-sm text-orange-600 dark:text-orange-400">
                          Alerts Enabled
                        </span>
                      </>
                    ) : (
                      <>
                        <BellOff className="h-4 w-4 text-gray-400" />
                        <span className="text-sm text-gray-600 dark:text-gray-400">
                          Alerts Disabled
                        </span>
                      </>
                    )}
                  </div>
                </div>
              </CardContent>
            </Card>

            {/* Tags */}
            {profile.tags && profile.tags.length > 0 && (
              <Card>
                <CardHeader>
                  <CardTitle>Tags</CardTitle>
                </CardHeader>
                <CardContent>
                  <div className="flex flex-wrap gap-2">
                    {profile.tags.map((tag, idx) => (
                      <Badge key={idx} variant="secondary">
                        {tag}
                      </Badge>
                    ))}
                  </div>
                </CardContent>
              </Card>
            )}

            {/* Activity Summary */}
            <Card>
              <CardHeader>
                <CardTitle>Activity Summary</CardTitle>
              </CardHeader>
              <CardContent className="space-y-3">
                <div className="flex items-center justify-between text-sm">
                  <span className="text-gray-600 dark:text-gray-400">Last Seen</span>
                  <span className="font-medium">{formatRelativeTime(profile.last_seen_at)}</span>
                </div>
                {profile.last_seen_camera_id && (
                  <div className="flex items-center justify-between text-sm">
                    <span className="text-gray-600 dark:text-gray-400">Last Camera</span>
                    <span className="font-medium">{profile.last_seen_camera_id}</span>
                  </div>
                )}
                <div className="flex items-center justify-between text-sm">
                  <span className="text-gray-600 dark:text-gray-400">Created</span>
                  <span className="font-medium">{formatRelativeTime(profile.created_at)}</span>
                </div>
                <div className="flex items-center justify-between text-sm">
                  <span className="text-gray-600 dark:text-gray-400">Last Updated</span>
                  <span className="font-medium">{formatRelativeTime(profile.updated_at)}</span>
                </div>
              </CardContent>
            </Card>
          </div>
        </div>
      </main>
    </div>
  );
}
