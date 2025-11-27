'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import {
  Shield,
  Plus,
  Search,
  Eye,
  Edit,
  Trash2,
  Users,
  Bell,
  BellOff,
  Webhook,
  Activity,
  AlertTriangle,
} from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Watchlist } from '@/lib/types';

export default function WatchlistsPage() {
  const [watchlists, setWatchlists] = useState<Watchlist[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [filterActive, setFilterActive] = useState<boolean | undefined>(undefined);

  useEffect(() => {
    loadWatchlists();
  }, []);

  const loadWatchlists = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getWatchlists({
        is_active: filterActive,
      });

      // Filter by search query client-side
      let filtered = data;
      if (searchQuery) {
        const query = searchQuery.toLowerCase();
        filtered = data.filter(
          (w) =>
            w.name.toLowerCase().includes(query) ||
            w.description?.toLowerCase().includes(query)
        );
      }

      setWatchlists(filtered);
    } catch (error) {
      console.error('Failed to load watchlists:', error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const debounce = setTimeout(() => {
      loadWatchlists();
    }, 300);
    return () => clearTimeout(debounce);
  }, [searchQuery, filterActive]);

  const handleDeleteWatchlist = async (watchlistId: string) => {
    if (!confirm('Are you sure you want to delete this watchlist?')) return;

    try {
      await apiClient.deleteWatchlist(watchlistId);
      loadWatchlists();
    } catch (error) {
      console.error('Failed to delete watchlist:', error);
      alert('Failed to delete watchlist');
    }
  };

  const handleToggleActive = async (watchlist: Watchlist) => {
    try {
      await apiClient.updateWatchlist(watchlist.id, {
        is_active: !watchlist.is_active,
      });
      loadWatchlists();
    } catch (error) {
      console.error('Failed to update watchlist:', error);
      alert('Failed to update watchlist');
    }
  };

  // Calculate total statistics
  const totalMembers = watchlists.reduce((sum, w) => sum + w.member_count, 0);
  const totalDetections = watchlists.reduce((sum, w) => sum + w.total_detections, 0);
  const activeWatchlists = watchlists.filter((w) => w.is_active).length;
  const watchlistsWithAlerts = watchlists.filter((w) => w.enable_alerts).length;

  return (
    <div className="min-h-screen bg-gray-50">
      <Header />
      <div className="container mx-auto py-8 px-4">
        {/* Page Header */}
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-2">
              <Shield className="h-8 w-8 text-blue-600" />
              Watchlists
            </h1>
            <p className="text-gray-500 mt-1">
              Manage watchlists for grouping profiles and setting custom alerts
            </p>
          </div>
          <Link href="/watchlists/new">
            <Button className="flex items-center gap-2">
              <Plus className="h-4 w-4" />
              Create Watchlist
            </Button>
          </Link>
        </div>

        {/* Statistics Cards */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Total Watchlists</p>
                  <p className="text-2xl font-bold">{watchlists.length}</p>
                </div>
                <Shield className="h-8 w-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Active</p>
                  <p className="text-2xl font-bold">{activeWatchlists}</p>
                </div>
                <Activity className="h-8 w-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Total Members</p>
                  <p className="text-2xl font-bold">{totalMembers}</p>
                </div>
                <Users className="h-8 w-8 text-purple-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">With Alerts</p>
                  <p className="text-2xl font-bold">{watchlistsWithAlerts}</p>
                </div>
                <Bell className="h-8 w-8 text-orange-500" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Filters */}
        <Card className="mb-6">
          <CardContent className="pt-6">
            <div className="flex flex-col md:flex-row gap-4">
              <div className="flex-1 relative">
                <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-gray-400" />
                <Input
                  placeholder="Search watchlists..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="pl-10"
                />
              </div>
              <div className="flex gap-2">
                <Button
                  variant={filterActive === undefined ? 'default' : 'outline'}
                  onClick={() => setFilterActive(undefined)}
                >
                  All
                </Button>
                <Button
                  variant={filterActive === true ? 'default' : 'outline'}
                  onClick={() => setFilterActive(true)}
                >
                  Active
                </Button>
                <Button
                  variant={filterActive === false ? 'default' : 'outline'}
                  onClick={() => setFilterActive(false)}
                >
                  Inactive
                </Button>
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Watchlists Grid */}
        {loading ? (
          <div className="text-center py-12">
            <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
            <p className="mt-2 text-gray-500">Loading watchlists...</p>
          </div>
        ) : watchlists.length === 0 ? (
          <Card>
            <CardContent className="py-12">
              <div className="text-center">
                <Shield className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                <h3 className="text-lg font-semibold mb-2">No watchlists found</h3>
                <p className="text-gray-500 mb-4">
                  {searchQuery
                    ? 'Try adjusting your search query'
                    : 'Create your first watchlist to get started'}
                </p>
                <Link href="/watchlists/new">
                  <Button>
                    <Plus className="h-4 w-4 mr-2" />
                    Create Watchlist
                  </Button>
                </Link>
              </div>
            </CardContent>
          </Card>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {watchlists.map((watchlist) => (
              <Card
                key={watchlist.id}
                className="hover:shadow-lg transition-shadow"
                style={{ borderLeft: `4px solid ${watchlist.color}` }}
              >
                <CardHeader>
                  <div className="flex items-start justify-between">
                    <div className="flex-1">
                      <CardTitle className="flex items-center gap-2">
                        {watchlist.icon && <span className="text-xl">{watchlist.icon}</span>}
                        {watchlist.name}
                      </CardTitle>
                      <p className="text-sm text-gray-500 mt-1 line-clamp-2">
                        {watchlist.description || 'No description'}
                      </p>
                    </div>
                    <div className="flex gap-1">
                      {watchlist.is_active ? (
                        <Badge variant="default" className="bg-green-500">
                          Active
                        </Badge>
                      ) : (
                        <Badge variant="secondary">Inactive</Badge>
                      )}
                    </div>
                  </div>
                </CardHeader>
                <CardContent>
                  {/* Statistics */}
                  <div className="grid grid-cols-2 gap-4 mb-4">
                    <div className="flex items-center gap-2">
                      <Users className="h-4 w-4 text-gray-400" />
                      <div>
                        <p className="text-xs text-gray-500">Members</p>
                        <p className="font-semibold">{watchlist.member_count}</p>
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <Activity className="h-4 w-4 text-gray-400" />
                      <div>
                        <p className="text-xs text-gray-500">Detections</p>
                        <p className="font-semibold">{watchlist.total_detections}</p>
                      </div>
                    </div>
                  </div>

                  {/* Features */}
                  <div className="flex flex-wrap gap-2 mb-4">
                    {watchlist.enable_alerts && (
                      <Badge variant="outline" className="text-xs flex items-center gap-1">
                        <Bell className="h-3 w-3" />
                        Alerts
                      </Badge>
                    )}
                    {watchlist.webhook_enabled && (
                      <Badge variant="outline" className="text-xs flex items-center gap-1">
                        <Webhook className="h-3 w-3" />
                        Webhook
                      </Badge>
                    )}
                    {watchlist.priority > 0 && (
                      <Badge variant="outline" className="text-xs flex items-center gap-1">
                        <AlertTriangle className="h-3 w-3" />
                        Priority {watchlist.priority}
                      </Badge>
                    )}
                  </div>

                  {/* Alert Settings Summary */}
                  {watchlist.enable_alerts && (
                    <div className="text-xs text-gray-500 mb-4 p-2 bg-gray-50 rounded">
                      <p>
                        Alert on:{' '}
                        {watchlist.alert_on_cameras.length > 0
                          ? `${watchlist.alert_on_cameras.length} camera(s)`
                          : 'all cameras'}
                      </p>
                      <p>Cooldown: {watchlist.alert_cooldown_minutes} min</p>
                    </div>
                  )}

                  {/* Actions */}
                  <div className="flex gap-2">
                    <Link href={`/watchlists/${watchlist.id}`} className="flex-1">
                      <Button variant="outline" className="w-full" size="sm">
                        <Eye className="h-4 w-4 mr-2" />
                        View
                      </Button>
                    </Link>
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={() => handleToggleActive(watchlist)}
                      title={watchlist.is_active ? 'Deactivate' : 'Activate'}
                    >
                      {watchlist.is_active ? (
                        <BellOff className="h-4 w-4" />
                      ) : (
                        <Bell className="h-4 w-4" />
                      )}
                    </Button>
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={() => handleDeleteWatchlist(watchlist.id)}
                      className="text-red-600 hover:bg-red-50"
                    >
                      <Trash2 className="h-4 w-4" />
                    </Button>
                  </div>
                </CardContent>
              </Card>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
