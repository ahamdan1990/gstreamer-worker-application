'use client';

import { Card, CardContent } from '@/components/ui/card';
import { Users } from 'lucide-react';
import { Header } from '@/components/header';

export default function WatchlistsPage() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Watchlists" description="Manage watchlists and profiles" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <Users className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Watchlists Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              Create and manage watchlists for face recognition
            </p>
          </CardContent>
        </Card>
      </main>
    </div>
  );
}
