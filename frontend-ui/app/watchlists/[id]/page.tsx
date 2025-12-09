'use client';

import { useEffect, useState } from 'react';
import { useParams, useRouter } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import {
  ArrowLeft,
  Edit,
  Trash2,
  Users,
  Bell,
  Webhook as WebhookIcon,
  Activity,
  AlertTriangle,
  Plus,
  X,
  Search,
  Loader2,
} from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Watchlist, Profile } from '@/lib/types';

export default function WatchlistViewPage() {
  const params = useParams();
  const router = useRouter();
  const watchlistId = params.id as string;

  const [watchlist, setWatchlist] = useState<Watchlist | null>(null);
  const [loading, setLoading] = useState(true);
  const [showAddProfile, setShowAddProfile] = useState(false);
  const [availableProfiles, setAvailableProfiles] = useState<Profile[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [loadingProfiles, setLoadingProfiles] = useState(false);

  useEffect(() => {
    loadWatchlist();
  }, [watchlistId]);

  const loadWatchlist = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getWatchlist(watchlistId);
      setWatchlist(data);
    } catch (error) {
      console.error('Failed to load watchlist:', error);
      alert('Failed to load watchlist');
      router.push('/watchlists');
    } finally {
      setLoading(false);
    }
  };

  const loadAvailableProfiles = async () => {
    try {
      setLoadingProfiles(true);
      const allProfiles = await apiClient.getProfiles({ is_active: true });

      // Filter out profiles already in watchlist
      const currentProfileIds = watchlist?.profiles?.map((p) => p.id) || [];
      const available = allProfiles.filter((p) => !currentProfileIds.includes(p.id));

      setAvailableProfiles(available);
      setShowAddProfile(true);
    } catch (error) {
      console.error('Failed to load profiles:', error);
    } finally {
      setLoadingProfiles(false);
    }
  };

  const handleAddProfile = async (profileId: string) => {
    try {
      await apiClient.addProfileToWatchlist(watchlistId, profileId);
      await loadWatchlist();
      setShowAddProfile(false);
      setSearchQuery('');
    } catch (error) {
      console.error('Failed to add profile:', error);
      alert('Failed to add profile to watchlist');
    }
  };

  const handleRemoveProfile = async (profileId: string) => {
    if (!confirm('Remove this profile from the watchlist?')) return;

    try {
      await apiClient.removeProfileFromWatchlist(watchlistId, profileId);
      await loadWatchlist();
    } catch (error) {
      console.error('Failed to remove profile:', error);
      alert('Failed to remove profile from watchlist');
    }
  };

  const handleDeleteWatchlist = async () => {
    if (!confirm('Are you sure you want to delete this watchlist? This action cannot be undone.')) return;

    try {
      await apiClient.deleteWatchlist(watchlistId);
      router.push('/watchlists');
    } catch (error) {
      console.error('Failed to delete watchlist:', error);
      alert('Failed to delete watchlist');
    }
  };

  const filteredProfiles = availableProfiles.filter((p) =>
    searchQuery
      ? p.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        p.compreface_subject_id.toLowerCase().includes(searchQuery.toLowerCase())
      : true
  );

  if (loading) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-center">
          <Loader2 className="h-12 w-12 animate-spin mx-auto text-blue-600" />
          <p className="mt-4 text-gray-600">Loading watchlist...</p>
        </div>
      </div>
    );
  }

  if (!watchlist) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray-600">Watchlist not found</p>
          <Link href="/watchlists">
            <Button className="mt-4">Back to Watchlists</Button>
          </Link>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50">
      <Header />

      <main className="container mx-auto px-4 py-8">
        {/* Header */}
        <div className="mb-6">
          <Link href="/watchlists">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Watchlists
            </Button>
          </Link>
        </div>

        <div className="flex justify-between items-start mb-6">
          <div>
            <div className="flex items-center gap-3 mb-2">
              {watchlist.icon && <span className="text-3xl">{watchlist.icon}</span>}
              <h1 className="text-3xl font-bold text-gray-900">{watchlist.name}</h1>
              <Badge variant={watchlist.is_active ? 'default' : 'secondary'} className="text-sm">
                {watchlist.is_active ? 'Active' : 'Inactive'}
              </Badge>
            </div>
            {watchlist.description && (
              <p className="text-gray-600">{watchlist.description}</p>
            )}
          </div>
          <div className="flex gap-2">
            <Link href={`/watchlists/${watchlistId}/edit`}>
              <Button variant="outline">
                <Edit className="h-4 w-4 mr-2" />
                Edit
              </Button>
            </Link>
            <Button variant="outline" onClick={handleDeleteWatchlist} className="text-red-600 hover:bg-red-50">
              <Trash2 className="h-4 w-4 mr-2" />
              Delete
            </Button>
          </div>
        </div>

        {/* Statistics */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Members</p>
                  <p className="text-2xl font-bold text-gray-900">{watchlist.member_count}</p>
                </div>
                <Users className="w-8 h-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Total Detections</p>
                  <p className="text-2xl font-bold text-gray-900">{watchlist.total_detections}</p>
                </div>
                <Activity className="w-8 h-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Priority</p>
                  <p className="text-2xl font-bold text-gray-900">{watchlist.priority}</p>
                </div>
                <AlertTriangle className="w-8 h-8 text-orange-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Alerts</p>
                  <p className="text-2xl font-bold text-gray-900">
                    {watchlist.enable_alerts ? 'Enabled' : 'Disabled'}
                  </p>
                </div>
                <Bell className={`w-8 h-8 ${watchlist.enable_alerts ? 'text-red-500' : 'text-gray-400'}`} />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Configuration Details */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
          <Card>
            <CardHeader>
              <CardTitle>Alert Configuration</CardTitle>
            </CardHeader>
            <CardContent className="space-y-3">
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Alerts Enabled</span>
                <Badge variant={watchlist.enable_alerts ? 'default' : 'secondary'}>
                  {watchlist.enable_alerts ? 'Yes' : 'No'}
                </Badge>
              </div>
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Alert Cooldown</span>
                <span className="text-sm font-medium">{watchlist.alert_cooldown_minutes} minutes</span>
              </div>
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Alert on Cameras</span>
                <span className="text-sm font-medium">
                  {watchlist.alert_on_cameras.length > 0
                    ? `${watchlist.alert_on_cameras.length} camera(s)`
                    : 'All cameras'}
                </span>
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Webhook Configuration</CardTitle>
            </CardHeader>
            <CardContent className="space-y-3">
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Webhook Enabled</span>
                <Badge variant={watchlist.webhook_enabled ? 'default' : 'secondary'}>
                  {watchlist.webhook_enabled ? 'Yes' : 'No'}
                </Badge>
              </div>
              {watchlist.webhook_enabled && watchlist.webhook_url && (
                <div>
                  <span className="text-sm text-gray-600">Webhook URL</span>
                  <p className="text-sm font-mono bg-gray-100 p-2 rounded mt-1 break-all">
                    {watchlist.webhook_url}
                  </p>
                </div>
              )}
            </CardContent>
          </Card>
        </div>

        {/* Members */}
        <Card>
          <CardHeader>
            <div className="flex justify-between items-center">
              <div>
                <CardTitle>Watchlist Members</CardTitle>
                <CardDescription>
                  Profiles assigned to this watchlist ({watchlist.member_count})
                </CardDescription>
              </div>
              <Button onClick={loadAvailableProfiles} disabled={loadingProfiles}>
                {loadingProfiles ? (
                  <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                ) : (
                  <Plus className="h-4 w-4 mr-2" />
                )}
                Add Profile
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            {/* Add Profile Modal */}
            {showAddProfile && (
              <div className="mb-4 p-4 border rounded-lg bg-blue-50">
                <div className="flex justify-between items-center mb-3">
                  <h3 className="font-semibold">Add Profile to Watchlist</h3>
                  <Button variant="ghost" size="sm" onClick={() => setShowAddProfile(false)}>
                    <X className="h-4 w-4" />
                  </Button>
                </div>
                <div className="relative mb-3">
                  <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-gray-400" />
                  <Input
                    placeholder="Search profiles..."
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    className="pl-10"
                  />
                </div>
                <div className="space-y-2 max-h-64 overflow-y-auto">
                  {filteredProfiles.length === 0 ? (
                    <p className="text-sm text-gray-500 text-center py-4">
                      {searchQuery ? 'No matching profiles found' : 'No available profiles'}
                    </p>
                  ) : (
                    filteredProfiles.map((profile) => (
                      <div
                        key={profile.id}
                        className="flex items-center justify-between p-2 bg-white rounded border hover:border-blue-300 cursor-pointer"
                        onClick={() => handleAddProfile(profile.id)}
                      >
                        <div>
                          <p className="font-medium text-sm">{profile.name}</p>
                          <p className="text-xs text-gray-500">{profile.compreface_subject_id}</p>
                        </div>
                        <Button variant="ghost" size="sm">
                          <Plus className="h-4 w-4" />
                        </Button>
                      </div>
                    ))
                  )}
                </div>
              </div>
            )}

            {/* Current Members */}
            {!watchlist.profiles || watchlist.profiles.length === 0 ? (
              <div className="text-center py-12">
                <Users className="h-12 w-12 text-gray-400 mx-auto mb-3" />
                <p className="text-gray-600 mb-2">No profiles assigned to this watchlist</p>
                <Button onClick={loadAvailableProfiles} variant="outline" size="sm">
                  <Plus className="h-4 w-4 mr-2" />
                  Add Your First Profile
                </Button>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                {watchlist.profiles.map((profile) => (
                  <Card key={profile.id} className="hover:shadow-md transition-shadow">
                    <CardContent className="pt-4">
                      <div className="flex items-start justify-between mb-2">
                        <div className="flex items-center gap-2">
                          <div className="w-10 h-10 rounded-full bg-gradient-to-br from-blue-500 to-purple-600 flex items-center justify-center text-white font-bold">
                            {profile.name.charAt(0).toUpperCase()}
                          </div>
                          <div>
                            <p className="font-semibold">{profile.name}</p>
                            <p className="text-xs text-gray-500">
                              {profile.compreface_subject_id.substring(0, 8)}...
                            </p>
                          </div>
                        </div>
                        <Button
                          variant="ghost"
                          size="sm"
                          onClick={() => handleRemoveProfile(profile.id)}
                          className="text-red-600 hover:bg-red-50"
                        >
                          <X className="h-4 w-4" />
                        </Button>
                      </div>
                      <div className="flex items-center justify-between text-xs text-gray-600">
                        <span>{profile.total_detections} detections</span>
                        <Badge variant={profile.is_active ? 'default' : 'secondary'} className="text-xs">
                          {profile.is_active ? 'Active' : 'Inactive'}
                        </Badge>
                      </div>
                    </CardContent>
                  </Card>
                ))}
              </div>
            )}
          </CardContent>
        </Card>
      </main>
    </div>
  );
}
