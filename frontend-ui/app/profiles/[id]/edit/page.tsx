'use client';

import { useEffect, useState, useRef } from 'react';
import { useRouter, useParams } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { Switch } from '@/components/ui/switch';
import { Badge } from '@/components/ui/badge';
import {
  ArrowLeft,
  Save,
  Loader2,
  Upload,
  X,
  Image as ImageIcon,
  RefreshCw,
  Trash2,
} from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Profile } from '@/lib/types';

const MAX_IMAGES = 5;

interface NewImage {
  file: File;
  preview: string;
}

export default function EditProfilePage() {
  const router = useRouter();
  const params = useParams();
  const profileId = params.id as string;
  const fileInputRef = useRef<HTMLInputElement>(null);

  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [profile, setProfile] = useState<Profile | null>(null);

  const [existingImages, setExistingImages] = useState<string[]>([]);
  const [newImages, setNewImages] = useState<NewImage[]>([]);
  const [imagesToDelete, setImagesToDelete] = useState<string[]>([]);

  const [tags, setTags] = useState<string[]>([]);
  const [tagInput, setTagInput] = useState('');

  const [formData, setFormData] = useState({
    name: '',
    description: '',
    email: '',
    phone: '',
    alert_on_recognition: false,
  });

  useEffect(() => {
    loadProfile();
  }, [profileId]);

  const loadProfile = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getProfile(profileId);
      setProfile(data);

      setFormData({
        name: data.name,
        description: data.description || '',
        email: data.email || '',
        phone: data.phone || '',
        alert_on_recognition: data.alert_on_recognition,
      });

      setExistingImages(data.images || []);
      setTags(data.tags || []);
    } catch (error) {
      console.error('Failed to load profile:', error);
      alert('Failed to load profile');
      router.push('/profiles');
    } finally {
      setLoading(false);
    }
  };

  const handleImageSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []);
    const totalImages = existingImages.length - imagesToDelete.length + newImages.length;
    const remainingSlots = MAX_IMAGES - totalImages;
    const filesToAdd = files.slice(0, remainingSlots);

    const addedImages: NewImage[] = filesToAdd.map((file) => ({
      file,
      preview: URL.createObjectURL(file),
    }));

    setNewImages([...newImages, ...addedImages]);

    // Reset input
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const handleRemoveNewImage = (index: number) => {
    const updated = [...newImages];
    URL.revokeObjectURL(updated[index].preview);
    updated.splice(index, 1);
    setNewImages(updated);
  };

  const handleMarkImageForDeletion = (imageUrl: string) => {
    if (imagesToDelete.includes(imageUrl)) {
      setImagesToDelete(imagesToDelete.filter((url) => url !== imageUrl));
    } else {
      setImagesToDelete([...imagesToDelete, imageUrl]);
    }
  };

  const handleAddTag = () => {
    if (tagInput.trim() && !tags.includes(tagInput.trim())) {
      setTags([...tags, tagInput.trim()]);
      setTagInput('');
    }
  };

  const handleRemoveTag = (tag: string) => {
    setTags(tags.filter((t) => t !== tag));
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    // Validation
    if (!formData.name.trim()) {
      alert('Please enter a name');
      return;
    }

    const finalImageCount =
      existingImages.length - imagesToDelete.length + newImages.length;

    if (finalImageCount === 0) {
      if (!confirm('No images will remain. Continue?')) {
        return;
      }
    }

    setSaving(true);

    try {
      // Update profile data
      const profileData = {
        name: formData.name,
        description: formData.description || undefined,
        email: formData.email || undefined,
        phone: formData.phone || undefined,
        alert_on_recognition: formData.alert_on_recognition,
        tags: tags.length > 0 ? tags : undefined,
      };

      await apiClient.updateProfile(profileId, profileData);

      // Delete marked images
      for (const imageUrl of imagesToDelete) {
        try {
          // Extract image ID from URL if needed
          const imageId = imageUrl.split('/').pop() || '';
          await fetch(`http://localhost:8002/api/v1/profiles/${profileId}/images/${imageId}`, {
            method: 'DELETE',
          });
        } catch (error) {
          console.error('Failed to delete image:', error);
        }
      }

      // Upload new images
      if (newImages.length > 0 && profile) {
        for (const image of newImages) {
          const imageFormData = new FormData();
          imageFormData.append('file', image.file);
          imageFormData.append('subject', profile.compreface_subject_id);

          try {
            await fetch(`http://localhost:8002/api/v1/profiles/${profileId}/images`, {
              method: 'POST',
              body: imageFormData,
            });
          } catch (error) {
            console.error('Failed to upload image:', error);
          }
        }
      }

      // Clean up previews
      newImages.forEach((img) => URL.revokeObjectURL(img.preview));

      router.push(`/profiles/${profileId}`);
    } catch (error) {
      console.error('Failed to update profile:', error);
      alert('Failed to update profile. Please check the form and try again.');
    } finally {
      setSaving(false);
    }
  };

  if (loading || !profile) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  const totalImages = existingImages.length - imagesToDelete.length + newImages.length;

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title={`Edit ${profile.name}`} description="Update profile information and images" />

      <main className="container mx-auto px-6 py-8 pb-20 max-w-4xl">
        <div className="mb-6">
          <Link href={`/profiles/${profileId}`}>
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Profile
            </Button>
          </Link>
        </div>

        <form onSubmit={handleSubmit} className="space-y-6">
          {/* Basic Information */}
          <Card>
            <CardHeader>
              <CardTitle>Basic Information</CardTitle>
              <CardDescription>Profile identification and contact details</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="space-y-2">
                <Label htmlFor="name">Name*</Label>
                <Input
                  id="name"
                  required
                  value={formData.name}
                  onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                  placeholder="e.g., John Doe"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="subject_id">CompreFace Subject ID (read-only)</Label>
                <Input
                  id="subject_id"
                  value={profile.compreface_subject_id}
                  disabled
                  className="bg-slate-100"
                />
                <p className="text-xs text-muted-foreground">
                  Subject ID cannot be changed after profile creation
                </p>
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  placeholder="Optional description or notes"
                  rows={3}
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="email">Email</Label>
                  <Input
                    id="email"
                    type="email"
                    value={formData.email}
                    onChange={(e) => setFormData({ ...formData, email: e.target.value })}
                    placeholder="email@example.com"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="phone">Phone</Label>
                  <Input
                    id="phone"
                    type="tel"
                    value={formData.phone}
                    onChange={(e) => setFormData({ ...formData, phone: e.target.value })}
                    placeholder="+1234567890"
                  />
                </div>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="alert">Alert on Recognition</Label>
                  <p className="text-sm text-muted-foreground">Send alerts when this person is detected</p>
                </div>
                <Switch
                  id="alert"
                  checked={formData.alert_on_recognition}
                  onCheckedChange={(checked) =>
                    setFormData({ ...formData, alert_on_recognition: checked })
                  }
                />
              </div>
            </CardContent>
          </Card>

          {/* Tags */}
          <Card>
            <CardHeader>
              <CardTitle>Tags</CardTitle>
              <CardDescription>Add labels for organization and filtering</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex gap-2">
                <Input
                  value={tagInput}
                  onChange={(e) => setTagInput(e.target.value)}
                  onKeyPress={(e) => {
                    if (e.key === 'Enter') {
                      e.preventDefault();
                      handleAddTag();
                    }
                  }}
                  placeholder="Enter a tag and press Enter"
                />
                <Button type="button" variant="outline" onClick={handleAddTag}>
                  Add Tag
                </Button>
              </div>

              {tags.length > 0 && (
                <div className="flex flex-wrap gap-2">
                  {tags.map((tag) => (
                    <Badge key={tag} variant="secondary" className="gap-1">
                      {tag}
                      <X
                        className="h-3 w-3 cursor-pointer"
                        onClick={() => handleRemoveTag(tag)}
                      />
                    </Badge>
                  ))}
                </div>
              )}
            </CardContent>
          </Card>

          {/* Face Images */}
          <Card>
            <CardHeader>
              <CardTitle>Face Images</CardTitle>
              <CardDescription>
                Manage images for face recognition (max {MAX_IMAGES}, current: {totalImages})
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="text-center">
                <input
                  ref={fileInputRef}
                  type="file"
                  accept="image/*"
                  multiple
                  onChange={handleImageSelect}
                  className="hidden"
                />

                <Button
                  type="button"
                  variant="outline"
                  onClick={() => fileInputRef.current?.click()}
                  disabled={totalImages >= MAX_IMAGES}
                  className="w-full"
                >
                  <Upload className="h-4 w-4 mr-2" />
                  Upload New Images ({totalImages}/{MAX_IMAGES})
                </Button>
              </div>

              {/* Existing Images */}
              {existingImages.length > 0 && (
                <div>
                  <h4 className="font-semibold mb-3">Existing Images</h4>
                  <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                    {existingImages.map((imageUrl, index) => {
                      const markedForDeletion = imagesToDelete.includes(imageUrl);
                      return (
                        <div key={index} className="relative group">
                          <div
                            className={`aspect-square rounded-lg overflow-hidden border-2 ${
                              markedForDeletion
                                ? 'border-red-500 opacity-50'
                                : 'border-gray-200 dark:border-gray-700'
                            }`}
                          >
                            <img
                              src={imageUrl}
                              alt={`Image ${index + 1}`}
                              className="w-full h-full object-cover"
                            />
                          </div>
                          <Button
                            type="button"
                            variant={markedForDeletion ? 'default' : 'destructive'}
                            size="icon"
                            className="absolute top-2 right-2 opacity-0 group-hover:opacity-100 transition-opacity"
                            onClick={() => handleMarkImageForDeletion(imageUrl)}
                          >
                            {markedForDeletion ? (
                              <RefreshCw className="h-4 w-4" />
                            ) : (
                              <Trash2 className="h-4 w-4" />
                            )}
                          </Button>
                          {markedForDeletion && (
                            <div className="absolute inset-0 flex items-center justify-center bg-black/50 rounded-lg">
                              <span className="text-white font-semibold">Will Delete</span>
                            </div>
                          )}
                          <div className="absolute bottom-2 left-2 bg-black/50 text-white text-xs px-2 py-1 rounded">
                            Image {index + 1}
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              )}

              {/* New Images */}
              {newImages.length > 0 && (
                <div>
                  <h4 className="font-semibold mb-3">New Images to Upload</h4>
                  <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                    {newImages.map((image, index) => (
                      <div key={index} className="relative group">
                        <div className="aspect-square rounded-lg overflow-hidden border-2 border-green-500">
                          <img
                            src={image.preview}
                            alt={`New ${index + 1}`}
                            className="w-full h-full object-cover"
                          />
                        </div>
                        <Button
                          type="button"
                          variant="destructive"
                          size="icon"
                          className="absolute top-2 right-2 opacity-0 group-hover:opacity-100 transition-opacity"
                          onClick={() => handleRemoveNewImage(index)}
                        >
                          <X className="h-4 w-4" />
                        </Button>
                        <div className="absolute bottom-2 left-2 bg-green-600 text-white text-xs px-2 py-1 rounded">
                          New {index + 1}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {existingImages.length === 0 && newImages.length === 0 && (
                <div className="border-2 border-dashed border-gray-300 dark:border-gray-700 rounded-lg p-12 text-center">
                  <ImageIcon className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                  <p className="text-gray-600 dark:text-gray-400">No images</p>
                  <p className="text-sm text-muted-foreground mt-1">
                    Upload face images for recognition
                  </p>
                </div>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href={`/profiles/${profileId}`}>
              <Button type="button" variant="outline">
                Cancel
              </Button>
            </Link>
            <Button type="submit" disabled={saving}>
              {saving ? (
                <>
                  <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                  Saving...
                </>
              ) : (
                <>
                  <Save className="h-4 w-4 mr-2" />
                  Save Changes
                </>
              )}
            </Button>
          </div>
        </form>
      </main>
    </div>
  );
}
