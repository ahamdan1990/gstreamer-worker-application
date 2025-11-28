'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import {
  User,
  Plus,
  Search,
  Eye,
  Edit,
  Trash2,
  Activity,
  Clock,
  Camera as CameraIcon,
  Bell,
  BellOff,
  Users,
  RefreshCcw
} from 'lucide-react';
import { Layout } from '@/components/layout';
import apiClient from '@/lib/api';
import wsClient from '@/lib/websocket';
import type { Profile, PersonEvent } from '@/lib/types';

export default function ProfilesPage() {
  const [profiles, setProfiles] = useState<Profile[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [filterActive, setFilterActive] = useState<boolean | undefined>(undefined);
  const [wsConnected, setWsConnected] = useState(false);
  const [syncing, setSyncing] = useState(false);

  useEffect(() => {
    loadProfiles();

    // Automatically sync CompreFace subjects on page load (silent)
    const syncOnLoad = async () => {
      try {
        await apiClient.syncComprefaceSubjects();
        // Reload profiles after sync
        loadProfiles();
      } catch (error) {
        console.log('Background CompreFace sync skipped:', error);
        // Silent failure - don't disrupt user experience
      }
    };
    syncOnLoad();

    const interval = setInterval(() => setWsConnected(wsClient.isConnected()), 1000);

    wsClient.connect();

    // Subscribe to person recognition events to update statistics in real-time
    const unsubscribe = wsClient.subscribeToPersonRecognized((event) => {
      setProfiles((prev) =>
        prev.map((profile) => {
          if (profile.compreface_subject_id === event.subject) {
            return {
              ...profile,
              total_detections: profile.total_detections + 1,
              last_seen_at: event.created_at || new Date().toISOString(),
              last_seen_camera_id: event.camera_id,
            };
          }
          return profile;
        })
      );
    });

    return () => {
      unsubscribe();
      clearInterval(interval);
    };
  }, []);

  const loadProfiles = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getProfiles({
        is_active: filterActive,
        search: searchQuery || undefined,
      });
      setProfiles(data);
    } catch (error) {
      console.error('Failed to load profiles:', error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const debounce = setTimeout(() => {
      loadProfiles();
    }, 300);
    return () => clearTimeout(debounce);
  }, [searchQuery, filterActive]);

  const handleDeleteProfile = async (profileId: string) => {
    if (!confirm('Are you sure you want to delete this profile?')) return;

    try {
      await apiClient.deleteProfile(profileId);
      loadProfiles();
    } catch (error) {
      console.error('Failed to delete profile:', error);
      alert('Failed to delete profile');
    }
  };

  const handleToggleActive = async (profile: Profile) => {
    try {
      await apiClient.updateProfile(profile.id, {
        is_active: !profile.is_active,
      });
      loadProfiles();
    } catch (error) {
      console.error('Failed to update profile:', error);
    }
  };

  const handleSyncCompreface = async () => {
    if (!confirm('This will sync all subjects from CompreFace and create profiles for any that don\'t exist yet. Continue?')) {
      return;
    }

    setSyncing(true);
    try {
      const result = await apiClient.syncComprefaceSubjects();

      let message = `Sync complete!\n\n`;
      message += `✓ Created ${result.synced} new profiles\n`;
      message += `• ${result.existing} subjects already had profiles\n`;

      if (result.failed > 0) {
        message += `✗ ${result.failed} subjects failed to sync\n`;
      }

      alert(message);
      loadProfiles();
    } catch (error: any) {
      console.error('Failed to sync CompreFace subjects:', error);
      const errorMsg = error.response?.data?.detail || error.message || 'Unknown error';
      alert(`Failed to sync CompreFace subjects: ${errorMsg}`);
    } finally {
      setSyncing(false);
    }
  };

  const formatDate = (dateStr?: string) => {
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

  const filteredProfiles = profiles;

  const stats = {
    total: profiles.length,
    active: profiles.filter((p) => p.is_active).length,
    autoCreated: profiles.filter((p) => p.auto_created).length,
    withAlerts: profiles.filter((p) => p.alert_on_recognition).length,
  };

  return (
    <Layout>
      <main className="container mx-auto px-4 py-8">
        {/* Header */}
        <div className="flex justify-between items-center mb-8">
          <div>
            <h1 className="text-3xl font-bold text-gray-900 dark:text-white mb-2">
              Profiles
            </h1>
            <p className="text-gray-600 dark:text-gray-400">
              Manage person profiles and face recognition
            </p>
          </div>
          <div className="flex gap-3">
            <Badge variant={wsConnected ? 'default' : 'destructive'}>
              {wsConnected ? 'Live' : 'Disconnected'}
            </Badge>
            <Button
              variant="outline"
              onClick={handleSyncCompreface}
              disabled={syncing}
            >
              <RefreshCcw className={`w-4 h-4 mr-2 ${syncing ? 'animate-spin' : ''}`} />
              {syncing ? 'Syncing...' : 'Sync CompreFace'}
            </Button>
            <Link href="/profiles/add">
              <Button>
                <Plus className="w-4 h-4 mr-2" />
                Add Profile
              </Button>
            </Link>
          </div>
        </div>

        {/* Stats Cards */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600 dark:text-gray-400">Total Profiles</p>
                  <p className="text-2xl font-bold text-gray-900 dark:text-white">{stats.total}</p>
                </div>
                <Users className="w-8 h-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600 dark:text-gray-400">Active</p>
                  <p className="text-2xl font-bold text-green-600">{stats.active}</p>
                </div>
                <Activity className="w-8 h-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600 dark:text-gray-400">Auto-Created</p>
                  <p className="text-2xl font-bold text-yellow-600">{stats.autoCreated}</p>
                </div>
                <User className="w-8 h-8 text-yellow-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600 dark:text-gray-400">With Alerts</p>
                  <p className="text-2xl font-bold text-red-600">{stats.withAlerts}</p>
                </div>
                <Bell className="w-8 h-8 text-red-500" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Search and Filters */}
        <Card className="mb-6">
          <CardContent className="pt-6">
            <div className="flex gap-4">
              <div className="flex-1 relative">
                <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 w-4 h-4 text-gray-400" />
                <Input
                  placeholder="Search profiles by name, email, or subject ID..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="pl-10"
                />
              </div>
              <select
                value={filterActive === undefined ? 'all' : filterActive ? 'active' : 'inactive'}
                onChange={(e) =>
                  setFilterActive(e.target.value === 'all' ? undefined : e.target.value === 'active')
                }
                className="px-4 py-2 border rounded-md bg-white dark:bg-gray-800"
              >
                <option value="all">All Profiles</option>
                <option value="active">Active Only</option>
                <option value="inactive">Inactive Only</option>
              </select>
            </div>
          </CardContent>
        </Card>

        {/* Profiles Grid */}
        {loading ? (
          <div className="text-center py-12">
            <div className="inline-block animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600"></div>
            <p className="mt-4 text-gray-600 dark:text-gray-400">Loading profiles...</p>
          </div>
        ) : filteredProfiles.length === 0 ? (
          <Card>
            <CardContent className="py-12 text-center">
              <User className="w-16 h-16 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-semibold text-gray-900 dark:text-white mb-2">
                No profiles found
              </h3>
              <p className="text-gray-600 dark:text-gray-400 mb-4">
                {searchQuery
                  ? 'Try adjusting your search query'
                  : 'Get started by adding your first profile'}
              </p>
              {!searchQuery && (
                <Link href="/profiles/add">
                  <Button>
                    <Plus className="w-4 h-4 mr-2" />
                    Add Profile
                  </Button>
                </Link>
              )}
            </CardContent>
          </Card>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {filteredProfiles.map((profile) => (
              <Card key={profile.id} className="hover:shadow-lg transition-shadow">
                <CardHeader>
                  <div className="flex items-start justify-between">
                    <div className="flex items-center gap-3">
                      <div className="w-12 h-12 rounded-full bg-gradient-to-br from-blue-500 to-purple-600 flex items-center justify-center text-white font-bold text-lg">
                        {profile.name.charAt(0).toUpperCase()}
                      </div>
                      <div>
                        <CardTitle className="text-lg">{profile.name}</CardTitle>
                        <p className="text-sm text-gray-600 dark:text-gray-400">
                          {profile.compreface_subject_id.substring(0, 8)}...
                        </p>
                      </div>
                    </div>
                    <div className="flex gap-1">
                      {profile.auto_created && (
                        <Badge variant="outline" className="text-xs">
                          Auto
                        </Badge>
                      )}
                      {profile.is_active ? (
                        <Badge variant="default" className="text-xs">
                          Active
                        </Badge>
                      ) : (
                        <Badge variant="secondary" className="text-xs">
                          Inactive
                        </Badge>
                      )}
                    </div>
                  </div>
                </CardHeader>

                <CardContent>
                  <div className="space-y-3">
                    {profile.description && (
                      <p className="text-sm text-gray-600 dark:text-gray-400 line-clamp-2">
                        {profile.description}
                      </p>
                    )}

                    <div className="grid grid-cols-2 gap-2 text-sm">
                      <div className="flex items-center gap-2">
                        <Activity className="w-4 h-4 text-gray-400" />
                        <span className="text-gray-600 dark:text-gray-400">
                          {profile.total_detections} detections
                        </span>
                      </div>
                      <div className="flex items-center gap-2">
                        <Clock className="w-4 h-4 text-gray-400" />
                        <span className="text-gray-600 dark:text-gray-400">
                          {formatDate(profile.last_seen_at)}
                        </span>
                      </div>
                    </div>

                    {profile.last_seen_camera_id && (
                      <div className="flex items-center gap-2 text-sm">
                        <CameraIcon className="w-4 h-4 text-gray-400" />
                        <span className="text-gray-600 dark:text-gray-400">
                          Last seen: {profile.last_seen_camera_id}
                        </span>
                      </div>
                    )}

                    {profile.avg_recognition_similarity > 0 && (
                      <div className="flex items-center gap-2">
                        <div className="flex-1 bg-gray-200 dark:bg-gray-700 rounded-full h-2">
                          <div
                            className="bg-blue-600 h-2 rounded-full"
                            style={{ width: `${profile.avg_recognition_similarity * 100}%` }}
                          />
                        </div>
                        <span className="text-xs text-gray-600 dark:text-gray-400">
                          {(profile.avg_recognition_similarity * 100).toFixed(0)}%
                        </span>
                      </div>
                    )}

                    {profile.alert_on_recognition && (
                      <div className="flex items-center gap-2 text-sm text-orange-600 dark:text-orange-400">
                        <Bell className="w-4 h-4" />
                        <span>Alerts enabled</span>
                      </div>
                    )}

                    {profile.tags && profile.tags.length > 0 && (
                      <div className="flex flex-wrap gap-1">
                        {profile.tags.map((tag, idx) => (
                          <Badge key={idx} variant="outline" className="text-xs">
                            {tag}
                          </Badge>
                        ))}
                      </div>
                    )}

                    <div className="flex gap-2 pt-2">
                      <Link href={`/profiles/${profile.id}`} className="flex-1">
                        <Button variant="outline" size="sm" className="w-full">
                          <Eye className="w-4 h-4 mr-1" />
                          View
                        </Button>
                      </Link>
                      <Link href={`/profiles/${profile.id}/edit`}>
                        <Button variant="outline" size="sm">
                          <Edit className="w-4 h-4" />
                        </Button>
                      </Link>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleToggleActive(profile)}
                      >
                        {profile.is_active ? (
                          <BellOff className="w-4 h-4" />
                        ) : (
                          <Bell className="w-4 h-4" />
                        )}
                      </Button>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleDeleteProfile(profile.id)}
                      >
                        <Trash2 className="w-4 h-4 text-red-500" />
                      </Button>
                    </div>
                  </div>
                </CardContent>
              </Card>
            ))}
          </div>
        )}
      </main>
    </Layout>
  );
}
