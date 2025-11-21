'use client';

import { Card, CardContent } from '@/components/ui/card';
import { TrendingUp } from 'lucide-react';
import { Header } from '@/components/header';

export default function AnalyticsPage() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Analytics" description="Camera analytics and insights" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <TrendingUp className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Analytics Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              View camera performance metrics, motion detection trends, and more
            </p>
          </CardContent>
        </Card>
      </main>
    </div>
  );
}
